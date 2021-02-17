/**
 * Copyright (C) 2019 Xilinx, Inc
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


#include <iostream>
#include <fstream>
#include <algorithm>
#include <climits>
#include <iomanip>
#include <memory>
#include <regex>
#include <sstream>
#include <stdint.h>
#include <cstring>
#include <vector>
#include <locale>

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include "boost/filesystem.hpp"
#include <boost/tokenizer.hpp>

#include "xclbin.h"
#include "core/common/utils.h"
#include "firmware_image.h"


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

bool DSAInfo::matchId(DSAInfo& dsa) const
{
    if (uuids.size() != dsa.uuids.size())
        return false;
    else if (uuids.size() == 0)
    {
        if (timestamp == dsa.timestamp)
            return true;
    }
    else if (uuids[0].compare(dsa.uuids[0]) == 0)
    {
        return true;
    }
    return false;
}

std::vector<DSAInfo> firmwareImage::getIntalledDSAs()
{
    std::vector<DSAInfo> installedDSA;
    // Obtain installed DSA info.
    std::vector<boost::filesystem::path> fw_dirs(FIRMWARE_DIRS);
    for (auto& root : fw_dirs) {
        if (!boost::filesystem::exists(root) || !boost::filesystem::is_directory(root))
            continue;

        boost::filesystem::recursive_directory_iterator end_iter;
        // for (auto const & iter : boost::filesystem::recursive_directory_iterator(root)) {
        for(boost::filesystem::recursive_directory_iterator iter(root); iter != end_iter; ++iter) {
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

firmwareImage::firmwareImage(const char *file, imageType type) :
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
        else
        {
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
            unsigned int mcsType = (type == MCS_FIRMWARE_PRIMARY) ? MCS_PRIMARY : MCS_SECONDARY;
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
    else
    {
        // For non-dsabin file, the entire file is the image.
        mBuf = new char[bufsize];
        in_file.seekg(0);
        in_file.read(mBuf, bufsize);
    }

// rdbuf doesn't work on windows and str() doesn't work for ospi_versal on linux
#ifdef __GNUC__
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
