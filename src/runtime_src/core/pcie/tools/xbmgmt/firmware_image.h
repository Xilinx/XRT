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
#define FORMATTED_FW_DIR    "/opt/xilinx/firmware"
#define DSA_FILE_SUFFIX     "mcs"
#define DSABIN_FILE_SUFFIX  "dsabin"
#define XSABIN_FILE_SUFFIX  "xsabin"
#define XCLBIN_FILE_SUFFIX  "xclbin"
#define NULL_TIMESTAMP      0

class DSAInfo
{
public:
    bool hasFlashImage;
    std::string vendor;
    std::string board;
    std::string name;
    std::string file;
    std::shared_ptr<char> dtbbuf;
    uint64_t timestamp;
    std::vector<std::string> uuids;
    std::string bmcVer;

    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subsystem_id;
    std::string partition_family_name;
    std::string partition_name;
    std::string build_ident;
    static const std::string UNKNOWN;
    static const std::string INACTIVE;

    DSAInfo(const std::string& filename, uint64_t ts, const std::string& id, const std::string& bmc);
    DSAInfo(const std::string& filename);
    DSAInfo(const std::string& filename, std::string& pr_board, std::string& pr_family, std::string& pr_name);
    ~DSAInfo();

    bool matchId(std::string& id);
    bool matchId(DSAInfo& dsa);
    bool matchIntId(std::string& id);
};

std::ostream& operator<<(std::ostream& stream, const DSAInfo& dsa);

enum imageType
{
    BMC_FIRMWARE,
    MCS_FIRMWARE_PRIMARY,
    MCS_FIRMWARE_SECONDARY,
    STRIPPED_FIRMWARE,
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

#endif
