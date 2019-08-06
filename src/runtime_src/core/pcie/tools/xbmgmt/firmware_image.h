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

/*
 * Contains definitions for all firmware (DSA/BMC) related classes
 */

#ifndef _FIRMWARE_IMAGE_H_
#define _FIRMWARE_IMAGE_H_

#include <sstream>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// directory where all MCS files are saved
#define FIRMWARE_DIR        "/lib/firmware/xilinx/"
#define DSA_FILE_SUFFIX     "mcs"
#define DSABIN_FILE_SUFFIX  "dsabin"
#define XSABIN_FILE_SUFFIX  "xsabin"
#define NULL_TIMESTAMP      0

class DSAInfo
{
public:
    bool DSAValid;
    std::string vendor;
    std::string board;
    std::string name;
    std::string file;
    uint64_t timestamp;
    std::string uuid;
    std::string bmcVer;

    DSAInfo(const std::string& filename, uint64_t ts, const std::string& id, const std::string& bmc);
    DSAInfo(const std::string& filename);
    ~DSAInfo();
};

std::ostream& operator<<(std::ostream& stream, const DSAInfo& dsa);

enum imageType
{
    BMC_FIRMWARE,
    MCS_FIRMWARE_PRIMARY,
    MCS_FIRMWARE_SECONDARY,
};

class firmwareImage : public std::istringstream
{
public:
    firmwareImage(const char *file, imageType type);
    ~firmwareImage();
    static std::vector<DSAInfo>& getIntalledDSAs();
private:
    static std::vector<DSAInfo> installedDSA;
    int mType;
    char *mBuf;
};

#define ALIGN(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((char *)(ALIGN((unsigned long)(p), (a))))
#define GET_CELL(p)     (p += 4, *((const uint32_t *)(p-4)))

#define FDT_BEGIN_NODE  0x1
#define FDT_END_NODE    0x2
#define FDT_PROP        0x3
#define FDT_NOP         0x4
#define FDT_END         0x9

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

#endif
