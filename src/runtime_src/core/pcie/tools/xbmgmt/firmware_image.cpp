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
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>
#include "firmware_image.h"
#include "xclbin.h"

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

void getTimestampFromFilename(std::string filename, uint64_t &ts)
{
    std::regex e(".*-([0-9a-fA-F]+)." DSABIN_FILE_SUFFIX);
    std::cmatch cm;

    std::regex_match(filename.c_str(), cm, e);
    if (cm.size() == 2) {
        std::stringstream ss;

        ss << std::hex << cm[1];
	ss >> ts;
    } else
        ts = NULL_TIMESTAMP;
}

static void uuid2ts(std::string& uuid, uint64_t& ts)
{
    std::string str(uuid, 0, 16);
    ts = strtoull(str.c_str(), nullptr, 16);
}

void getIntUUIDFromDTB(void *blob, uint64_t &ts, std::string &uuid)
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

        if (!strcmp(s, "interface_uuid"))
        {
            uuid = std::string(p);
	    uuid2ts(uuid, ts);
            return;
        }
        p = PALIGN(p + sz, 4);
    }
}

DSAInfo::DSAInfo(const std::string& filename, uint64_t ts, const std::string& id, const std::string& bmc) :
    DSAValid(false), vendor(), board(), name(), file(filename),
    timestamp(ts), uuid(id), bmcVer(bmc)
{
    if (filename.empty())
        return;

    size_t dotpos = filename.rfind(".");
    size_t slashpos = filename.rfind("/");

    // Just DSA name.
    if (dotpos == std::string::npos)
    {
        name = filename;
        getVendorBoardFromDSAName(name, vendor, board);
        if (!uuid.empty() && !timestamp)
            uuid2ts(uuid, timestamp);
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
        DSAValid = true;
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
        name.assign(reinterpret_cast<const char *>(ap->m_header.m_platformVBNV));
        // Normalize DSA name: v:b:n:a.b -> v_b_n_a_b
        std::replace_if(name.begin(), name.end(),
            [](const char &a){ return a == ':' || a == '.'; }, '_');
        getVendorBoardFromDSAName(name, vendor, board);
        getTimestampFromFilename(filename, timestamp);
        // For 2RP platform, only UUIDs are provided
        // Assume there is only 1 interface UUID is provided for BLP,
        // Show it as ID for flashing
        const axlf_section_header* dtbSection = xclbin::get_axlf_section(ap, PARTITION_METADATA);
        if (dtbSection && timestamp == NULL_TIMESTAMP) {
            std::shared_ptr<char> dtbbuf(new char[dtbSection->m_sectionSize]);
            in.seekg(dtbSection->m_sectionOffset);
            in.read(dtbbuf.get(), dtbSection->m_sectionSize);
	    getIntUUIDFromDTB(dtbbuf.get(), timestamp, uuid);
        }
        //timestamp = ap->m_header.m_featureRomTimeStamp;
        DSAValid = (xclbin::get_axlf_section(ap, MCS) != nullptr);

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

DSAInfo::~DSAInfo()
{
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
            if (!dsa.DSAValid)
                continue;
            installedDSA.push_back(dsa);
        }
        closedir(dp);
    }

    return installedDSA;
}

std::ostream& operator<<(std::ostream& stream, const DSAInfo& dsa)
{
    std::ios_base::fmtflags f(stream.flags());
    stream << dsa.name;
    if (dsa.timestamp != NULL_TIMESTAMP)
    {
        stream << ",[ID=0x" << std::hex << std::setw(16) << std::setfill('0')
            << dsa.timestamp << "]";
    }
    if (!dsa.bmcVer.empty())
    {
        stream << ",[SC=" << dsa.bmcVer << "]";
    }
    stream.flags(f);
    return stream;
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
        (fn.find("." DSABIN_FILE_SUFFIX) != std::string::npos))
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
            std::shared_ptr<char> bmcbuf(new char[bmcSection->m_sectionSize]);
            in.seekg(bmcSection->m_sectionOffset);
            in.read(bmcbuf.get(), bmcSection->m_sectionSize);
            if (!in.good())
            {
                this->setstate(failbit);
                std::cout << "Can't read SC section from "<< file << std::endl;
                return;
            }
            const struct bmc *bmc = reinterpret_cast<const struct bmc *>(bmcbuf.get());
            // Load data into stream.
            bufsize = bmc->m_size;
            mBuf = new char[bufsize];
            in.seekg(bmcSection->m_sectionOffset + bmc->m_offset);
            in.read(mBuf, bufsize);
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
    else
    {
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
