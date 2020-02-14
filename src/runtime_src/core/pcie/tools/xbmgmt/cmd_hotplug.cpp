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

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "xbmgmt.h"
#include "core/pcie/linux/scan.h"

#define SYSFS_PATH      "/sys/bus/pci"

static int rescanDevice(void);
static int removeDevice(unsigned int index);

const char *subCmdHotplugDesc = "Perform managed hotplug on the xilinx device";
const char *subCmdHotplugUsage = "--offline bdf | --online";

int hotplugHandler(int argc, char *argv[])
{
    sudoOrDie();

    if (argc < 2)
        return -EINVAL;
    
    int ret = 0;
    unsigned int index = UINT_MAX;
    int isRemove = 0;
    int isRescan = 0;

    const static option opts[] = {
        { "offline", required_argument, nullptr, '0' },
        { "online", no_argument, nullptr, '1' },
        { nullptr, 0, nullptr, 0 },
    };

    while (true) 
    {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);

        if (opt == -1)
            break;

        switch (opt) 
        {
            case '0':
                index = bdf2index(optarg);
                if (index == UINT_MAX)
                    return -ENOENT;
               
                isRemove = 1; 
                break;

            case '1':
                isRescan = 1;
                break;

            default:
                return -EINVAL;
        } 
    }
    
    /* Get permission from user. */
    std::cout << "CAUTION: Performing hotplug command. " <<
        "This command is going to impact both user pf and mgmt pf.\n" <<
        "Please make sure no application is currently running." << std::endl;

    if(!canProceed())
        return -ECANCELED;

    if (isRemove)
    {
        /* Rescan from /sys/bus/pci/<Endpoint>/remove */
        ret = removeDevice(index);
        if (ret)
            return ret;
    }

    if (isRescan)
    {
        /* Rescan from /sys/bus/pci/rescan */
        ret = rescanDevice();
        if (ret)
            return ret;
    }

    return ret;
}

static int removeDevice(unsigned int index)
{
    int ret = 0;
    auto mgmt_dev = pcidev::get_dev(index, false);

    /* Remove both user_pf and mgmt_pf */
    ret = mgmt_dev->shutdown(true, true);
    if (ret) {
        std::cout << "Removing device faied. " << ret << std::endl;
        return ret;
    }

    return 0;
}

static int rescanDevice(void)
{
    boost::filesystem::ofstream ofile;
    std::string iPath = SYSFS_PATH;
    
    iPath += "/rescan";
    if (iPath.empty()) 
        return -ENOENT;

    try
    {
        ofile.open(iPath);
        if (!ofile.is_open()) 
        {
            std::cout << "Failed to open " << iPath << ":" << strerror(errno) << std::endl;
            return -errno;
        }

        ofile << 1;
        ofile.flush();
        if (!ofile.good()) 
        {
            std::cout << "Failed to write " << iPath << ":"  << strerror(errno) << std::endl;
            ofile.close();
            return -errno;
        }
    }
    catch (const boost::filesystem::ifstream::failure& err) 
    {
        std::cout << "Exception!!!! " << err.what();
        ofile.close();
        return -errno;
    }

    ofile.close();

    return 0;
}
