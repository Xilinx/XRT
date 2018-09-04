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
    std::string bmcVer;

    DSAInfo(const std::string& filename, uint64_t ts, const std::string& bmc);
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

#endif
