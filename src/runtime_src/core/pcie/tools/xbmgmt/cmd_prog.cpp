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

#include <string>
#include <iostream>
#include <fstream>
#include <climits>
#include <getopt.h>

#include "xbmgmt.h"
#include "core/pcie/linux/scan.h"
#include "xclbin.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"

const char *subCmdProgDesc = "Download xclbin onto the device";
const char *subCmdProgUsage = "--path xclbin [--card bdf] [--force]";

int program(unsigned index, const std::string& xclbin)
{
    std::ifstream stream(xclbin.c_str());

    if(!stream.is_open()) {
        std::cout << "ERROR: Cannot open " << xclbin << std::endl;
        return -ENOENT;
    }

    stream.seekg(0, stream.end);
    int length = stream.tellg();
    stream.seekg(0, stream.beg);

    char *buffer = new char[length];
    stream.read(buffer, length);
    xclmgmt_ioc_bitstream_axlf obj = { reinterpret_cast<axlf *>(buffer) };
    auto dev = pcidev::get_dev(index, false);
    int ret = dev->ioctl(XCLMGMT_IOCICAPDOWNLOAD_AXLF, &obj);
    delete [] buffer;

    return ret ? -errno : ret;
}

int progHandler(int argc, char *argv[])
{
    sudoOrDie();

    if (argc < 2)
        return -EINVAL;

    unsigned index = UINT_MAX;
    bool force = false;
    std::string file;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "force", no_argument, nullptr, '1' },
        { "path", required_argument, nullptr, '2' },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            index = bdf2index(optarg);
            if (index == UINT_MAX)
                return -ENOENT;
            break;
        case '1':
            force = true;
            break;
        case '2':
            file = std::string(optarg);
            break;
        default:
            return -EINVAL;
        }
    }

    if (file.empty())
        return -EINVAL;

    if (index == UINT_MAX)
        index = 0;

    /* Get permission from user. */
    if (!force) {
        std::cout << "CAUTION: Downloading xclbin. " <<
            "Please make sure xocl driver is unloaded." << std::endl;
        if(!canProceed())
            return -ECANCELED;
    }

    return program(index, file);
}
