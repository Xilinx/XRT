/**
 * Copyright (C) 2018 Xilinx, Inc
 * Author(s): Max Zhen (maxz@xilinx.com)
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
#include <fstream>
#include <algorithm>
#include <climits>
#include <iomanip>
#include <memory>
#include <regex>
#include <sstream>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include "boost/filesystem.hpp"
#include "xclbin.h"
#include "firmware_image.h"
#include "core/pcie/linux/scan.h"
#include "xclbin.h"
#include "core/common/utils.h"

#define hex_digit "([0-9a-fA-F]+)"

using namespace boost::filesystem;

const std::string DSAInfo::UNKNOWN = "UNKNOWN";
const std::string DSAInfo::INACTIVE = "INACTIVE";

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

void parseDSAFilename(std::string filename, uint16_t& vendor, uint16_t& device, uint16_t& subsystem, uint64_t &ts)
{
    std::vector<std::string> suffix = { XSABIN_FILE_SUFFIX, DSABIN_FILE_SUFFIX};

    for (std::string t : suffix) {
        std::regex e(".*/([0-9a-fA-F]+)-([0-9a-fA-F]+)-([0-9a-fA-F]+)-([0-9a-fA-F]+)." + t);
        std::cmatch cm;

        std::regex_match(filename.c_str(), cm, e);
        if (cm.size() == 5) {
            vendor = std::stoull(cm.str(1), 0, 16);
            device = std::stoull(cm.str(2), 0, 16);
            subsystem = std::stoull(cm.str(3), 0, 16);
            ts = std::stoull(cm.str(4), 0, 16);
            break;
        } else
            ts = NULL_TIMESTAMP;
    }
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

DSAInfo::DSAInfo(const std::string& filename, uint64_t ts, const std::string& id, const std::string& bmc) :
    hasFlashImage(false), vendor(), board(), name(), file(filename),
    timestamp(ts), bmcVer(bmc),
    vendor_id(0), device_id(0), subsystem_id(0)
{
    size_t dotpos;
    size_t slashpos;
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
        std::ifstream in(file);
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
        if (std::strlen(reinterpret_cast<const char *>(ap->m_header.m_platformVBNV)) > 0)
        {
            name.assign(reinterpret_cast<const char *>(ap->m_header.m_platformVBNV));
        }
        // Normalize DSA name: v:b:n:a.b -> v_b_n_a_b
        std::replace_if(name.begin(), name.end(),
            [](const char &a){ return a == ':' || a == '.'; }, '_');
        getVendorBoardFromDSAName(name, vendor, board);
        parseDSAFilename(filename, vendor_id, device_id, subsystem_id, timestamp);
        
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
        hasFlashImage = (xclbin::get_axlf_section(ap, MCS) != nullptr) ||
            (xclbin::get_axlf_section(ap, ASK_FLASH) != nullptr) || (xclbin::get_axlf_section(ap, PDI) != nullptr);

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
        name = "xilinx_" + board + "_" + pr_family + "_" + pr_name;
}

DSAInfo::~DSAInfo()
{
}

bool DSAInfo::matchId(std::string &id)
{
    uint64_t ts = strtoull(id.c_str(), nullptr, 0);
    if (ts != 0 && ts != ULLONG_MAX && ts == timestamp)
        return true;

    if (uuids.size() > 0)
    {
        std::string uuid(id.length(), 0);
        std::transform(id.begin(), id.end(), uuid.begin(), ::tolower);
        std::string::size_type i = uuid.find("0x");
        if (i == 0)
            uuid.erase(0, 2);
        if (!strncmp(uuids[0].c_str(), uuid.c_str(), uuid.length()))
            return true;
    }

    return false;
}

bool DSAInfo::matchIntId(std::string &id)
{
    uint64_t ts = strtoull(id.c_str(), nullptr, 0);

    if (uuids.size() > 1)
    {
        std::string uuid(id.length(), 0);
        std::transform(id.begin(), id.end(), uuid.begin(), ::tolower);
        std::string::size_type i = uuid.find("0x");
        if (i == 0)
            uuid.erase(0, 2);
        for(unsigned int j = 1; j < uuids.size(); j++)
        {
            if (!strncmp(uuids[j].c_str(), uuid.c_str(), uuid.length()))
                return true;
            uint64_t int_ts = 0;
            uuid2ts(id, int_ts);
	    if (int_ts == ts)
                return true;
        }
    }
    return false;
}

bool DSAInfo::matchId(DSAInfo& dsa)
{
    if (uuids.size() == 0 && dsa.uuids.size() == 0 &&
        timestamp == dsa.timestamp)
        return true;

    //logid_uuid should always be the 1st.
    if (uuids.size() > 0 && dsa.uuids.size() > 0 &&
        uuids[0].compare(dsa.uuids[0]) == 0)
        return true;
    return false;
}

bool DSAInfo::bmcVerIsFixed()
{
	return (bmcVer.find("FIXED") != std::string::npos);
}

std::vector<DSAInfo> firmwareImage::installedDSA;

std::vector<DSAInfo>& firmwareImage::getIntalledDSAs()
{
    if (!installedDSA.empty())
        return installedDSA;

    struct dirent *entry;
    DIR *dp;
    std::string nm;

    // Obtain installed DSA info.
    dp = opendir(FIRMWARE_DIR);
    if (dp)
    {
        while ((entry = readdir(dp)))
        {
            std::string d(FIRMWARE_DIR);
            std::string e(entry->d_name);

            /*
             * First look for from .xsabin, if failed,
             * look for .dsabin file.
             * legacy .mcs file is not supported.
             */
            if ((e.find(XSABIN_FILE_SUFFIX) == std::string::npos) &&
                (e.find(DSABIN_FILE_SUFFIX) == std::string::npos))
                continue;

            DSAInfo dsa(d + e);
            installedDSA.push_back(dsa);
        }
        closedir(dp);
    }

    dp = opendir(FORMATTED_FW_DIR);
    if (!dp)
        return installedDSA;
    closedir(dp);

    path formatted_fw_dir(FORMATTED_FW_DIR);
    std::vector<std::string> suffix = { XSABIN_FILE_SUFFIX, DSABIN_FILE_SUFFIX};

    for (std::string t : suffix) {

        std::regex e("^" FORMATTED_FW_DIR "/([^/]+)/([^/]+)/([^/]+)/.+\\." + t);
        std::cmatch cm;

        for (recursive_directory_iterator iter(formatted_fw_dir, symlink_option::recurse), end;
            iter != end;
            )
        {
            std::string name = iter->path().string();
            std::regex_match(name.c_str(), cm, e);
            if (cm.size() > 0)
            {
                std::string pr_board = cm.str(1);
                std::string pr_family = cm.str(2);
                std::string pr_name = cm.str(3);
                DSAInfo dsa(name, pr_board, pr_family, pr_name);
                installedDSA.push_back(dsa);
                while (iter != end && iter.level() > 2)
                    iter.pop();
            } else if (iter.level() > 4)
                iter.pop();
            else
            {
                dp = opendir(name.c_str());
		if (!dp)
                {
                    iter.no_push();
                }
		else
                {
                    iter.no_push(false);
                    closedir(dp);
                }
                ++iter;
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

/**
 * Helper method to find a set of bytes in a given buffer.
 * 
 * @param pBuffer The buffer to be examined.
 * @param _bufferSize
 *                The size of the buffer.
 * @param _searchString
 *                The string to search for.
 * @param _foundOffset
 *                The offset where the string was found.
 * 
 * @return true  - The string was found;
 *         false - the string was not found
 */
static bool findBytesInBuffer(const char * pBuffer, uint64_t _bufferSize,
                  const std::string& _searchString, 
                  unsigned int& _foundOffset) {

  // Initialize return values
  _foundOffset = 0;

  // Working variables
  unsigned int stringLength = (unsigned int) _searchString.length();
  unsigned int matchIndex = 0;

  for (unsigned int index = 0; index < _bufferSize; ++index) {
    char aChar = pBuffer[index];
    if (aChar == _searchString[matchIndex++]) {
      if (matchIndex == stringLength) {
        _foundOffset = 1 + index - stringLength;
        return true;
      }
    } else {
      matchIndex = 0;
    }
  }

  return false;
}

static void remove_xsabin_mirror(void * pXsaBinBuffer)
{
  static const std::string MIRROR_DATA_START = "XCLBIN_MIRROR_DATA_START";
  static const std::string MIRROR_DATA_END = "XCLBIN_MIRROR_DATA_END";

  axlf *axlf_header = (struct axlf *) pXsaBinBuffer;
  uint64_t bufferSize = axlf_header->m_header.m_length;

  unsigned int startOffset;
  if (findBytesInBuffer((const char *) pXsaBinBuffer, bufferSize, MIRROR_DATA_START, startOffset) == false) 
    return;   // No MIRROR DATA

  unsigned int endOffset;
  if (findBytesInBuffer((const char *) pXsaBinBuffer, bufferSize, MIRROR_DATA_END, endOffset) == false) 
    return;   // Badly formed mirror data (we have a start, but no end)
  endOffset += MIRROR_DATA_END.length();

  if (endOffset <= startOffset)
    return;   // Tags are in the wrong order

  // Zero out memory (not really needed but done for completeness)
  uint64_t bytesRemoved = endOffset - startOffset;
  std::memset((unsigned char *) pXsaBinBuffer + startOffset, 0, bytesRemoved);

  // Compress the image
  uint64_t bytesToCopy = bufferSize - endOffset;
  if (bytesToCopy != 0) 
    std::memcpy((unsigned char *) pXsaBinBuffer + startOffset, (unsigned char *) pXsaBinBuffer + endOffset, bytesToCopy);

  // Update length of the buffer
  axlf_header->m_header.m_length = bufferSize - bytesRemoved;
}

static void remove_xsabin_section(void * pXsaBinBuffer, enum axlf_section_kind sectionToRemove)
{
  // Simple DRC check
  if (pXsaBinBuffer == nullptr)
    throw std::runtime_error("ERROR: Buffer pointer is a nullptr.");

  axlf *axlf_header = (struct axlf *) pXsaBinBuffer;

  // This loop does need to re-evaluate the m_numSections for it will be 
  // reduced as sections are removed.  In addition, we need to start again from
  // the start for there could be multiple sections of the same type that
  // need to be removed.
  for (unsigned index = 0; index < axlf_header->m_header.m_numSections; ++index) {
    axlf_section_header *sectionHeaderArray = &axlf_header->m_sections[0];

    // Is this a section of interest, if not then go to the next section
    if (sectionHeaderArray[index].m_sectionKind != sectionToRemove)
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
      std::memcpy((unsigned char *) pXsaBinBuffer + startToOffset, (unsigned char *) pXsaBinBuffer + startFromOffset, bytesToCopy);
    }

    // -- Now do some incremental clean up of the data structures
    // Update the length and offsets AFTER this entry
    axlf_header->m_header.m_length -= bytesRemoved;
    for (unsigned idx = index + 1; idx < axlf_header->m_header.m_numSections; ++idx) {
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
    uint64_t bytesToShift = axlf_header->m_header.m_length - ((unsigned char *) ptrStartFrom - (unsigned char *) pXsaBinBuffer);

    std::memcpy((unsigned char *) ptrStartTo, (unsigned char *) ptrStartFrom, bytesToShift);

    // Update data elements
    axlf_header->m_header.m_numSections -= 1;
    axlf_header->m_header.m_length -= sizeof(axlf_section_header);
    for (unsigned idx = 0; idx < axlf_header->m_header.m_numSections; ++idx) {
      sectionHeaderArray[idx].m_sectionOffset -= sizeof(axlf_section_header);
    }
  }
}

firmwareImage::firmwareImage(const char *file, imageType type) :
    mType(type), mBuf(nullptr)
{
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in.is_open())
    {
        this->setstate(failbit);
        std::cout << "Can't open " << file << std::endl;
        return;
    }
    streampos bufsize = in.tellg();
    in.seekg(0);

    std::string fn(file);
    if ((fn.find("." XSABIN_FILE_SUFFIX) != std::string::npos) ||
        (fn.find("." DSABIN_FILE_SUFFIX) != std::string::npos) ||
        (fn.find("." XCLBIN_FILE_SUFFIX) != std::string::npos))
    {
        // Read axlf from dsabin file to find out number of sections in total.
        axlf a;
        size_t sz = sizeof (axlf);
        in.read(reinterpret_cast<char *>(&a), sz);
        if (!in.good())
        {
            this->setstate(failbit);
            std::cout << "Can't read axlf from "<< file << std::endl;
            return;
        }

        // Reread axlf from dsabin file, including all section headers.

        // Sanity check for number of sections coming from user input file
        if (a.m_header.m_numSections > 10000)
            return;

        in.seekg(0);
        sz = sizeof (axlf) + sizeof (axlf_section_header) * (a.m_header.m_numSections - 1);
        std::vector<char> top(sz);
        in.read(top.data(), sz);
        if (!in.good())
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
            std::vector<char> bmcbuf(bmcSection->m_sectionSize);
            in.seekg(bmcSection->m_sectionOffset);
            in.read(bmcbuf.data(), bmcSection->m_sectionSize);
            if (!in.good())
            {
                this->setstate(failbit);
                std::cout << "Can't read SC section from "<< file << std::endl;
                return;
            }
            const struct bmc *bmc = reinterpret_cast<const struct bmc *>(bmcbuf.data());
            // Load data into stream.
            bufsize = bmc->m_size;
            mBuf = new char[bufsize];
            in.seekg(bmcSection->m_sectionOffset + bmc->m_offset);
            in.read(mBuf, bufsize);
        }
        else if (type == STRIPPED_FIRMWARE)
        {
            std::vector<char> full(ap->m_header.m_length);
            const axlf *fp = reinterpret_cast<const axlf *>(full.data());
            try {
                in.seekg(0);
                in.read(full.data(), full.size());
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
                    std::cout << "FLASH dsabin supports only primary bitstream: "
                        << file << std::endl;
                    return;
                }

                //load 'struct flash'
                struct flash flashMeta;
                in.seekg(flashSection->m_sectionOffset);
                in.read(reinterpret_cast<char *>(&flashMeta), sizeof(flashMeta));
                if (!in.good() || flashMeta.m_flash_type != FLT_BIN_PRIMARY)
                {
                    this->setstate(failbit);
                    std::cout << "Can't read FLASH section from "<< file << std::endl;
                    return;
                }
                // Load data into stream.
                bufsize = flashMeta.m_image_size;
                mBuf = new char[bufsize];
                in.seekg(flashSection->m_sectionOffset + flashMeta.m_image_offset);
                in.read(mBuf, bufsize);
            } 
            else if (pdiSection) {
                if (type != MCS_FIRMWARE_PRIMARY)
                {
                    this->setstate(failbit);
                    std::cout << "PDI dsabin supports only primary bitstream: "
                        << file << std::endl;
                    return;
                }

                // Load entire PDI section.
                std::vector<char> pdibuf(pdiSection->m_sectionSize);
                in.seekg(pdiSection->m_sectionOffset);
                in.read(pdibuf.data(), pdiSection->m_sectionSize);
                if (!in.good())
                {
                    this->setstate(failbit);
                    std::cout << "Can't read PDI section from "<< file << std::endl;
                    return;
                }
                bufsize = pdiSection->m_sectionSize;
                mBuf = new char[bufsize];
                in.seekg(pdiSection->m_sectionOffset);
                in.read(mBuf, bufsize);
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
                in.seekg(mcsSection->m_sectionOffset);
                in.read(mcsbuf.get(), mcsSection->m_sectionSize);
                if (!in.good())
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
                in.seekg(mcsSection->m_sectionOffset + c->m_offset);
                in.read(mBuf, bufsize);
            }
        }
    }
    else
    {
        if (type != BMC_FIRMWARE && type != MCS_FIRMWARE_PRIMARY)
        {
            this->setstate(failbit);
            std::cout << "non-dsabin supports only primary bitstream: "
                << file << std::endl;
            return;
        }
        // For non-dsabin file, the entire file is the image.
        mBuf = new char[bufsize];
        in.seekg(0);
        in.read(mBuf, bufsize);
    }
    this->rdbuf()->pubsetbuf(mBuf, bufsize);
}

firmwareImage::~firmwareImage()
{
    delete[] mBuf;
}
