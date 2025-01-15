/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "firmware_image.h"
#include "core/common/utils.h"
#include "core/include/xrt/detail/xclbin.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <climits>
#include <iomanip>
#include <memory>
#include <cstdint>
#include <cstring>
#include <vector>

#ifdef _WIN32
# pragma warning( disable : 4189 )
#define be32toh ntohl
#endif

#define hex_digit "([0-9a-fA-F]+)"

//from scan.h

#define FDT_BEGIN_NODE  0x1
#define FDT_END_NODE    0x2
#define FDT_PROP        0x3
#define FDT_NOP         0x4
#define FDT_END         0x9

#define ALIGN(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((char *)(ALIGN((unsigned long long)(p), (a))))
#define GET_CELL(p)     (p += 4, *((const uint32_t *)(p-4)))

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

/*
 * Helper to parse DSA name string and retrieve all tokens
 * The DSA name string is passed in by value since it'll be modified inside.
 */
std::vector<std::string> DSANameParser(std::string name)
{
    std::vector<std::string> tokens;
    std::string delimiter = "_";

    size_t pos = 0;
    std::string token;
    while ((pos = name.find(delimiter)) != std::string::npos)
    {
        token = name.substr(0, pos);
        tokens.push_back(token);
        name.erase(0, pos + delimiter.length());
    }
    tokens.push_back(name);
    return tokens;
}

void getVendorBoardFromDSAName(std::string& dsa, std::string& vendor, std::string& board)
{
    std::vector<std::string> tokens = DSANameParser(dsa);

    // At least, we need vendor.board
    if (tokens.size() < 2)
        return;
    vendor = tokens[0];
    board = tokens[1];
}



void parseDSAFilename(const std::string& filename, uint64_t& vendor, uint64_t& device, uint64_t& subsystem, uint64_t &ts)
{
    vendor = 0; device = 0; subsystem = 0; ts = 0;
    using tokenizer = boost::tokenizer< boost::char_separator<char> >;
    boost::char_separator<char> sep("-.");
    tokenizer tokens(filename, sep);
    int radix = 16;

	// check if we have 5 tokens: vendor, device, subsystem, ts, "dsabin"/"xsabin"
	if (std::distance(tokens.begin(), tokens.end()) == 5) {
	    tokenizer::iterator tok_iter = tokens.begin();
		vendor = std::stoull(std::string(*tok_iter), nullptr, radix);
		tok_iter++;
		device = std::stoull(std::string(*tok_iter), nullptr, radix);
		tok_iter++;
		subsystem = std::stoull(std::string(*tok_iter), nullptr, radix);
		tok_iter++;
		ts = std::stoull(std::string(*tok_iter), nullptr, radix);
		tok_iter++;
	} else
		ts = NULL_TIMESTAMP;
}

static void uuid2ts(const std::string& uuid, uint64_t& ts)
{
    std::string str(uuid, 0, 16);
    ts = strtoull(str.c_str(), nullptr, 16);
}

void getUUIDFromDTB(void *blob, uint64_t &ts, std::vector<std::string> &uuids)
{
    struct fdt_header *bph = (struct fdt_header *)blob;
    uint32_t version = be32toh(bph->version);
    uint32_t off_dt = be32toh(bph->off_dt_struct);
    const char *p_struct = (const char *)blob + off_dt;
    uint32_t off_str = be32toh(bph->off_dt_strings);
    const char *p_strings = (const char *)blob + off_str;
    const char *p, *s;
    uint32_t tag;
    int sz;

    p = p_struct;
    uuids.clear();
    while ((tag = be32toh(GET_CELL(p))) != FDT_END)
    {
        if (tag == FDT_BEGIN_NODE)
        {
            s = p;
            p = PALIGN(p + strlen(s) + 1, 4);
            continue;
        }

        if (tag != FDT_PROP)
            continue;

        sz = be32toh(GET_CELL(p));
        s = p_strings + be32toh(GET_CELL(p));
        if (version < 16 && sz >= 8)
            p = PALIGN(p, 8);

        if (!strcmp(s, "logic_uuid"))
        {
            uuids.insert(uuids.begin(), std::string(p));
        }
	else if (!strcmp(s, "interface_uuid"))
        {
            uuids.push_back(std::string(p));
        }
        p = PALIGN(p + sz, 4);
    }
    if (uuids.size() > 0)
        uuid2ts(uuids[0], ts);
}

DSAInfo::DSAInfo(const std::string& filename, uint64_t ts, const std::string& id, const std::string& bmcV) :
    hasFlashImage(false), vendor(), board(), name(), file(filename),
    timestamp(ts), bmcVer(bmcV),
    vendor_id(0), device_id(0), subsystem_id(0)
{
    size_t dotpos = 0;
    size_t slashpos = 0;
    if (!filename.empty())
    {
        dotpos = filename.rfind(".");
        slashpos = filename.rfind("/");
    }

    // Just DSA name.
    if (filename.empty() || dotpos == std::string::npos)
    {
        name = filename;
        getVendorBoardFromDSAName(name, vendor, board);
        if (!id.empty() && !timestamp)
        {
            uuids.push_back(id);
            auto installedDSAs = firmwareImage::getIntalledDSAs();
            for (DSAInfo& dsa: installedDSAs)
	    {
                if (dsa.uuids.size() > 0 && id.compare(dsa.uuids[0]) == 0)
                {
                    name = dsa.name;
                    if (!name.empty())
                    {
                        getVendorBoardFromDSAName(name, vendor, board);
                    }
                    vendor_id = dsa.vendor_id;
                    device_id = dsa.device_id;
                    subsystem_id = dsa.subsystem_id;
                    partition_family_name = dsa.partition_family_name;
                    partition_name = dsa.partition_name;
		    file = dsa.file;
                    break;
                }
            }

            uuid2ts(id, timestamp);
        }
        return;
    }

    std::string dsa = filename.substr(slashpos + 1, dotpos - slashpos - 1);
    std::string suffix = filename.substr(dotpos + 1);

    // MCS file path.
    if (suffix.compare(DSA_FILE_SUFFIX) == 0)
    {
        // Don't care about xxx_secondary.mcs files.
        if (dsa.find("secondary") != std::string::npos)
            return;
        // Don't include _primary in dsa name.
        size_t p = dsa.rfind("primary");
        if (p != std::string::npos)
            dsa.erase(p - 1); // remove the delimiter too
        name = dsa;
        getVendorBoardFromDSAName(name, vendor, board);
        hasFlashImage = true;
    }
    // DSABIN file path.
    else if ((suffix.compare(XSABIN_FILE_SUFFIX) == 0) ||
             (suffix.compare(DSABIN_FILE_SUFFIX) == 0))
    {
        std::ifstream in(file, std::ios::binary);
        if (!in.is_open())
        {
            std::cout << "Can't open " << filename << std::endl;
            return;
        }

        // Read axlf from dsabin file to find out number of sections in total.
        axlf a;
        size_t sz = sizeof (axlf);
        in.read(reinterpret_cast<char *>(&a), sz);
        if (!in.good())
        {
            std::cout << "Can't read axlf from "<< filename << std::endl;
            return;
        }

        // Reread axlf from dsabin file, including all sections headers.

        // Sanity check for number of sections coming from user input file
        if (a.m_header.m_numSections > 10000)
            return;

        sz = sizeof (axlf) + sizeof (axlf_section_header) * (a.m_header.m_numSections - 1);
        std::vector<char> top(sz);
        in.seekg(0);
        in.read(top.data(), sz);
        if (!in.good())
        {
            std::cout << "Can't read axlf and section headers from "<< filename << std::endl;
            return;
        }

        // Fill out DSA info.
        const axlf *ap = reinterpret_cast<const axlf *>(top.data());
        if (name.empty())
        {
            name.assign(reinterpret_cast<const char *>(ap->m_header.m_platformVBNV));
        }
        // Normalize DSA name: v:b:n:a.b -> v_b_n_a_b
        std::replace_if(name.begin(), name.end(),
            [](const char &a){ return a == ':' || a == '.'; }, '_');
        getVendorBoardFromDSAName(name, vendor, board);

        // get filename without the path
        using tokenizer = boost::tokenizer< boost::char_separator<char> >;
        boost::char_separator<char> sep("\\/");
        tokenizer tokens(filename, sep);
        std::string dsafile = "";
        for (auto tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
        	if ((std::string(*tok_iter).find(XSABIN_FILE_SUFFIX) != std::string::npos)
                || (std::string(*tok_iter).find(DSABIN_FILE_SUFFIX) != std::string::npos))
                dsafile = *tok_iter;
        }
        parseDSAFilename(dsafile, vendor_id, device_id, subsystem_id, timestamp);
        // Assume there is only 1 interface UUID is provided for BLP,
        // Show it as ID for flashing
        const axlf_section_header* dtbSection = xclbin::get_axlf_section(ap, PARTITION_METADATA);
        if (dtbSection && timestamp == NULL_TIMESTAMP) {
            dtbbuf = std::shared_ptr<char>(new char[dtbSection->m_sectionSize]);
            in.seekg(dtbSection->m_sectionOffset);
            in.read(dtbbuf.get(), dtbSection->m_sectionSize);
            getUUIDFromDTB(dtbbuf.get(), timestamp, uuids);
        }
        // For 2RP platform, only UUIDs are provided
        //timestamp = ap->m_header.m_featureRomTimeStamp;
        hasFlashImage = (xclbin::get_axlf_section(ap, MCS) != nullptr) || (xclbin::get_axlf_section(ap, PDI) != nullptr) ||
                            (xclbin::get_axlf_section(ap, ASK_FLASH) != nullptr);

        // Find out BMC version
        // Obtain BMC section header.
        const axlf_section_header* bmcSection = xclbin::get_axlf_section(ap, BMC);
        if (bmcSection == nullptr)
            return;
        // Load entire BMC section.
        std::shared_ptr<char> bmcbuf(new char[bmcSection->m_sectionSize]);
        in.seekg(bmcSection->m_sectionOffset);
        in.read(bmcbuf.get(), bmcSection->m_sectionSize);
        if (!in.good())
        {
            std::cout << "Can't read SC section from "<< filename << std::endl;
            return;
        }
        const struct bmc *bmc = reinterpret_cast<const struct bmc *>(bmcbuf.get());
        bmcVer = std::move(std::string(bmc->m_version));
    }
}

DSAInfo::DSAInfo(const std::string& filename) : DSAInfo(filename, NULL_TIMESTAMP, "", "")
{
}

DSAInfo::DSAInfo(const std::string& filename, std::string &pr_board, std::string& pr_family, std::string& pr_name) : DSAInfo(filename)
{
    vendor = "xilinx";
    board = pr_board;
    partition_family_name = pr_family;
    partition_name = pr_name;

    if (name.empty())
        name = boost::str(boost::format("xilinx_%s_%s_%s") % board % pr_family % pr_name);
}

DSAInfo::~DSAInfo()
{
}

static std::string
normalize_uuid(std::string id)
{
    // convert:
    // 0xB772B6BBD3BA046439ECE1B7763C69C7 -> b772b6bbd3ba046439ece1b7763c69c7
    std::string uuid = boost::algorithm::to_lower_copy(id);
    std::string::size_type i = uuid.find("0x");
    if (i == 0)
        uuid.erase(0, 2);
    return uuid;
}

bool DSAInfo::matchId(const std::string &id) const
{
    uint64_t ts = strtoull(id.c_str(), nullptr, 0);
    if (ts != 0 && ts != ULLONG_MAX && ts == timestamp)
        return true;

    if (uuids.empty()) {
        const std::string uuid = normalize_uuid(id);

        if (!strncmp(uuids[0].c_str(), uuid.c_str(), uuid.length()))
            return true;
    }

    return false;
}

bool DSAInfo::matchIntId(std::string &id) const
{
    uint64_t ts = strtoull(id.c_str(), nullptr, 0); //get timestamp
    const std::string uuid = normalize_uuid(id); //get hex UUID

    if (uuids.size() > 1) {
        for(unsigned int j = 1; j < uuids.size(); j++) {

            //Check 1: check if passed in id matches UUID
            if (!strncmp(uuids[j].c_str(), uuid.c_str(), uuid.length()))
                return true;

            //Check 2: check if passed in ID macthes the timestamp
            uint64_t int_ts = 0;
            uuid2ts(uuids[j], int_ts);
	        if (int_ts == ts)
                return true;
        }
    }
    return false;
}

bool DSAInfo::matchId(const DSAInfo& dsa) const
{
    if (uuids.empty() && dsa.uuids.empty() &&
        timestamp == dsa.timestamp)
        return true;

    //logid_uuid should always be the 1st.
    if (!uuids.empty() && !dsa.uuids.empty() &&
        uuids[0].compare(dsa.uuids[0]) == 0)
        return true;
    return false;
}

bool DSAInfo::bmcVerIsFixed() const
{
	return (bmcVer.find("FIXED") != std::string::npos);
}

std::vector<DSAInfo> firmwareImage::getIntalledDSAs()
{
    std::vector<DSAInfo> installedDSA;
    // Obtain installed DSA info.
    for (auto root : FIRMWARE_DIRS) {
      try {
        if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root))
            continue;
      }
      catch (const std::exception&) {
        // directory cannot be checked, maybe relative and
        // insufficient CWD permissions
        continue;
      }

      std::filesystem::recursive_directory_iterator end_iter;
      // for (auto const & iter : std::filesystem::recursive_directory_iterator(root)) {
      for(std::filesystem::recursive_directory_iterator iter(root); iter != end_iter; ++iter) {
        if ((iter->path().extension() == ".xsabin" || iter->path().extension() == ".dsabin")) {
          DSAInfo dsa(iter->path().string());
          installedDSA.push_back(dsa);
        }
      }
    }

    return installedDSA;
}

std::ostream& operator<<(std::ostream& stream, const DSAInfo& dsa)
{
    auto format = xrt_core::utils::ios_restore(stream);
    stream << dsa.name;
    if (dsa.timestamp != NULL_TIMESTAMP)
    {
        stream << ",[ID=0x" << std::hex << dsa.timestamp << "]";
    }
    if (!dsa.bmcVer.empty())
    {
        stream << ",[SC=" << dsa.bmcVer << "]";
    }
    return stream;
}

static void
remove_xsabin_mirror(void * xsabin_buffer)
{
  static const std::string mirror_data_start = "XCLBIN_MIRROR_DATA_START";
  static const std::string mirror_data_end = "XCLBIN_MIRROR_DATA_END";

  axlf *axlf_header = reinterpret_cast<struct axlf *>(xsabin_buffer);
  uint64_t bufferSize = axlf_header->m_header.m_length;

  std::stringstream strm;
  strm << xsabin_buffer;
  std::string str_xsabin_buffer = strm.str();

  auto start_offset = str_xsabin_buffer.find(mirror_data_start);
  if (start_offset == std::string::npos)
    return; // No MIRROR DATA

  auto end_offset = str_xsabin_buffer.find(mirror_data_end);
  if (end_offset == std::string::npos)
    return; // Badly formed mirror data (we have a start, but no end)

  if (end_offset <= start_offset)
    return;   // Tags are in the wrong order

  // Zero out memory (not really needed but done for completeness)
  uint64_t bytesRemoved = end_offset - start_offset;
  std::memset(reinterpret_cast<unsigned char *>(xsabin_buffer) + start_offset, 0, bytesRemoved);

  // Compress the image
  uint64_t bytesToCopy = bufferSize - end_offset;
  if (bytesToCopy != 0)
    std::memcpy(reinterpret_cast<unsigned char *>(xsabin_buffer) + start_offset, reinterpret_cast<unsigned char *>(xsabin_buffer) + end_offset, bytesToCopy);

  // Update length of the buffer
  axlf_header->m_header.m_length = bufferSize - bytesRemoved;
}

static void
remove_xsabin_section(void * xsabin_buffer, enum axlf_section_kind section_to_remove)
{
  // Simple DRC check
  if (xsabin_buffer == nullptr)
    throw std::runtime_error("ERROR: Buffer pointer is a nullptr.");
  axlf *axlf_header = reinterpret_cast<struct axlf *>(xsabin_buffer);

  // This loop does need to re-evaluate the m_numSections for it will be
  // reduced as sections are removed.  In addition, we need to start again from
  // the start for there could be multiple sections of the same type that
  // need to be removed.
  for (uint64_t index = 0; index < axlf_header->m_header.m_numSections; ++index) {
    axlf_section_header *sectionHeaderArray = &axlf_header->m_sections[0];
    // Is this a section of interest, if not then go to the next section
    if (sectionHeaderArray[index].m_sectionKind != static_cast<uint32_t>(section_to_remove))
      continue;
    // Record the buffer size
    uint64_t bufferSize = axlf_header->m_header.m_length;

    // Determine the data to be removed.
    uint64_t startToOffset = sectionHeaderArray[index].m_sectionOffset;
    uint64_t startFromOffset = ((index + 1) == axlf_header->m_header.m_numSections) ?
                                sectionHeaderArray[index].m_sectionOffset + sectionHeaderArray[index].m_sectionSize:
                                sectionHeaderArray[index + 1].m_sectionOffset;
    uint64_t bytesToCopy = bufferSize - startFromOffset;
    uint64_t bytesRemoved = startFromOffset - startToOffset;

    if (bytesToCopy != 0) {
      std::memcpy(reinterpret_cast<unsigned char *>(xsabin_buffer) + startToOffset,
                    reinterpret_cast<unsigned char *>(xsabin_buffer) + startFromOffset, bytesToCopy);
    }
    // -- Now do some incremental clean up of the data structures
    // Update the length and offsets AFTER this entry
    axlf_header->m_header.m_length -= bytesRemoved;
    for (uint64_t idx = index + 1; idx < axlf_header->m_header.m_numSections; ++idx) {
      sectionHeaderArray[idx].m_sectionOffset -= bytesRemoved;
    }
    // Are we removing the last section.  If so just update the count and leave
    if (axlf_header->m_header.m_numSections == 1) {
      axlf_header->m_header.m_numSections = 0;
      sectionHeaderArray[0].m_sectionKind = 0;
      sectionHeaderArray[0].m_sectionOffset = 0;
      sectionHeaderArray[0].m_sectionSize = 0;
      continue;
    }
    // Remove the array entry
    void * ptrStartTo = &sectionHeaderArray[index];
    void * ptrStartFrom = &sectionHeaderArray[index+1];
    uint64_t bytesToShift = axlf_header->m_header.m_length -
                            (reinterpret_cast<unsigned char *>(ptrStartFrom) - reinterpret_cast<unsigned char *>(xsabin_buffer));
    std::memcpy(reinterpret_cast<unsigned char *>(ptrStartTo), reinterpret_cast<unsigned char *>(ptrStartFrom), bytesToShift);

    // Update data elements
    axlf_header->m_header.m_numSections -= 1;
    axlf_header->m_header.m_length -= sizeof(axlf_section_header);
    for (uint64_t idx = 0; idx < axlf_header->m_header.m_numSections; ++idx) {
      sectionHeaderArray[idx].m_sectionOffset -= sizeof(axlf_section_header);
    }
  }
}

firmwareImage::firmwareImage(const std::string& file, imageType type) :
    mType(type), mBuf(nullptr)
{
    std::ifstream in_file(file, std::ios::binary | std::ios::ate);
    if (!in_file.is_open())
    {
        this->setstate(failbit);
        std::cout << "Can't open " << file << std::endl;
        return;
    }
    auto bufsize = in_file.tellg();
    in_file.seekg(0);

    std::string fn(file);
    if ((fn.find("." XSABIN_FILE_SUFFIX) != std::string::npos) ||
        (fn.find("." DSABIN_FILE_SUFFIX) != std::string::npos))
    {
        // Read axlf from dsabin file to find out number of sections in total.
        axlf a;
        size_t sz = sizeof (axlf);
        in_file.read(reinterpret_cast<char *>(&a), sz);
        if (!in_file.good())
        {
            this->setstate(failbit);
            std::cout << "Can't read axlf from "<< file << std::endl;
            return;
        }

        // Reread axlf from dsabin file, including all section headers.

        // Sanity check for number of sections coming from user input file
        if (a.m_header.m_numSections > 10000)
            return;

        in_file.seekg(0);
        sz = sizeof (axlf) + sizeof (axlf_section_header) * (a.m_header.m_numSections - 1);
        std::vector<char> top(sz);
        in_file.read(top.data(), sz);
        if (!in_file.good())
        {
            this->setstate(failbit);
            std::cout << "Can't read axlf and section headers from "<< file << std::endl;
            return;
        }

        const axlf *ap = reinterpret_cast<const axlf *>(top.data());
        if (type == BMC_FIRMWARE)
        {
            // Obtain BMC section header.
            const axlf_section_header* bmcSection = xclbin::get_axlf_section(ap, BMC);
            if (bmcSection == nullptr)
            {
                this->setstate(failbit);
                std::cout << "Can't find SC section in "<< file << std::endl;
                return;
            }
            // Load entire BMC section.
            std::shared_ptr<char> bmcbuf(new char[bmcSection->m_sectionSize]);
            in_file.seekg(bmcSection->m_sectionOffset);
            in_file.read(bmcbuf.get(), bmcSection->m_sectionSize);
            if (!in_file.good())
            {
                this->setstate(failbit);
                std::cout << "Can't read SC section from "<< file << std::endl;
                return;
            }
            const struct bmc *bmc = reinterpret_cast<const struct bmc *>(bmcbuf.get());
            // Load data into stream.
            bufsize = bmc->m_size;
            mBuf = new char[bufsize];
            in_file.seekg(bmcSection->m_sectionOffset + bmc->m_offset);
            in_file.read(mBuf, bufsize);
        }
        else if (type == STRIPPED_FIRMWARE)
        {
            std::vector<char> full(ap->m_header.m_length);
            const axlf *fp = reinterpret_cast<const axlf *>(full.data());
            try {
                in_file.seekg(0);
                in_file.read(full.data(), full.size());
                remove_xsabin_section(full.data(), ASK_FLASH);
                remove_xsabin_section(full.data(), PDI);
                remove_xsabin_section(full.data(), MCS);
                remove_xsabin_mirror(full.data());
            } catch (const std::exception &e) {
                this->setstate(failbit);
                std::cout << "failed to remove section from "<< file << ": "
                    << e.what() << std::endl;
                return;
            }
            // Load data into stream.
            bufsize = fp->m_header.m_length;
            mBuf = new char[bufsize];
            std::memcpy(mBuf, full.data(), bufsize);
        }
        else
        {
            //The new introduced FLASH section may contain either MCS or BIN, but not both,
            //if we see neither of them, it may be a legacy xsabin where MCS section is still
            //used to save the flash image.

            // Obtain FLASH section header.
            const axlf_section_header* flashSection = xclbin::get_axlf_section(ap, ASK_FLASH);
            const axlf_section_header* pdiSection = xclbin::get_axlf_section(ap, PDI);
            if (flashSection) {
                //So far, there is only one type in FLASH section.
                //Just blindly load that section. Add more checks later.

                if (type != MCS_FIRMWARE_PRIMARY)
                {
                    this->setstate(failbit);
                    return;
                }

                //load 'struct flash'
                struct flash flashMeta;
                in_file.seekg(flashSection->m_sectionOffset);
                in_file.read(reinterpret_cast<char *>(&flashMeta), sizeof(flashMeta));
                if (!in_file.good() || flashMeta.m_flash_type != FLT_BIN_PRIMARY)
                {
                    this->setstate(failbit);
                    std::cout << "Can't read FLASH section from "<< file << std::endl;
                    return;
                }
                // Load data into stream.
                bufsize = flashMeta.m_image_size;
                mBuf = new char[bufsize];
                in_file.seekg(flashSection->m_sectionOffset + flashMeta.m_image_offset);
                in_file.read(mBuf, bufsize);
            }
            else if (pdiSection) {
                if (type != MCS_FIRMWARE_PRIMARY)
                {
                    this->setstate(failbit);
                    std::cout << "PDI dsabin supports only primary bitstream: "
                        << file << std::endl;
                    return;
                }

                /*
                 * By default, we load entire xsabin.
		 * For legacy ospiversal type, the Flasher class will trim to PDI.
		 * For new ospi_xgq type, the Flasher will take entire xsabin.
                 */
                mBuf = new char[bufsize];
                in_file.seekg(0);
                in_file.read(mBuf, bufsize);
            } else {
                // Obtain MCS section header.
                const axlf_section_header* mcsSection = xclbin::get_axlf_section(ap, MCS);
                if (mcsSection == nullptr)
                {
                    this->setstate(failbit);
                    std::cout << "Can't find MCS section in "<< file << std::endl;
                    return;
                }
                // Load entire MCS section.
                std::shared_ptr<char> mcsbuf(new char[mcsSection->m_sectionSize]);
                in_file.seekg(mcsSection->m_sectionOffset);
                in_file.read(mcsbuf.get(), mcsSection->m_sectionSize);
                if (!in_file.good())
                {
                    this->setstate(failbit);
                    std::cout << "Can't read MCS section from "<< file << std::endl;
                    return;
                }
                const struct mcs *mcs = reinterpret_cast<const struct mcs *>(mcsbuf.get());
                // Only two types of MCS supported today
                unsigned mcsType = (type == MCS_FIRMWARE_PRIMARY) ? MCS_PRIMARY : MCS_SECONDARY;
                const struct mcs_chunk *c = nullptr;
                for (int8_t i = 0; i < mcs->m_count; i++)
                {
                    if (mcs->m_chunk[i].m_type == mcsType)
                    {
                        c = &mcs->m_chunk[i];
                        break;
                    }
                }
                if (c == nullptr)
                {
                    this->setstate(failbit);
                    return;
                }
                // Load data into stream.
                bufsize = c->m_size;
                mBuf = new char[bufsize];
                in_file.seekg(mcsSection->m_sectionOffset + c->m_offset);
                in_file.read(mBuf, bufsize);
            }
        }

    }
    else
    {
        if ((type != BMC_FIRMWARE) && (type != MCS_FIRMWARE_PRIMARY))
        {
            this->setstate(failbit);
            std::cout << "non-dsabin supports only primary bitstream: " << file << std::endl;
            return;
        }
        // For non-dsabin file, the entire file is the image.
        mBuf = new char[bufsize];
        in_file.seekg(0);
        in_file.read(mBuf, bufsize);
    }

// rdbuf doesn't work on windows and str() doesn't work for ospi_versal on linux
#ifdef __linux__
    this->rdbuf()->pubsetbuf(mBuf, bufsize);
#endif
#ifdef _WIN32
    this->str(mBuf);
#endif
}

firmwareImage::~firmwareImage()
{
    delete[] mBuf;
}
