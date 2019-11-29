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
#include <dirent.h>
#include <string.h>

#include <boost/filesystem.hpp>

#include "xbmgmt.h"
#include "core/pcie/linux/scan.h"

using namespace boost::filesystem;

#define SYSFS_PATH      "/sys/bus/pci/devices"
#define XILINX_VENDOR   "0x10ee"
#define XILINX_US       "0x9134"

static std::string findRootPort(void);
static int removeDevice(std::shared_ptr<pcidev::pci_device> dev);

const char *subCmdHpRemoveDesc = "Perform managed hot remove on the device";
const char *subCmdHpRemoveUsage = "--card b:d.f";

int hpRemoveHandler(int argc, char *argv[])
{
    sudoOrDie();

    unsigned index = UINT_MAX;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
    };

    if (argc != 3)
        return -EINVAL;

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
                break;

            default:
                return -EINVAL;
        } 
    }

    /* Remove user_pf */
    auto uDev = pcidev::get_dev(index, true);
    removeDevice(uDev);

    /* Remove mgmt_pf */
    auto mDev = pcidev::get_dev(index, false);
    removeDevice(mDev);
    
    return 0;
}

static int removeDevice(std::shared_ptr<pcidev::pci_device> dev)
{
    std::string sysfs_path = dev->get_sysfs_path("", "remove");

    if (sysfs_path.empty()) {
        return -ENOENT;
    }

    std::ofstream ofile(sysfs_path);

    if (!ofile.is_open()) {
        std::cout << "Failed to open " << sysfs_path << ":" << strerror(errno) << std::endl;
        return -ENOENT;
    }

    /* "echo 1 > /sys/bus/pci/<EndPoint>/remove" to trigger hot remove of the device */
    ofile << 1;
    ofile.flush();
    if (!ofile.good()) {
        std::cout << "Failed to write " << sysfs_path << ":"  << strerror(errno) << std::endl;
        ofile.close();
        return -EINVAL;
    }

    ofile.close();

    return 0;
}


const char *subCmdHpRescanDesc = "Perform hot rescan on the device";
const char *subCmdHpRescanUsage = "(no options supported)";

int hpRescanHandler(int argc, char *argv[])
{
    sudoOrDie();

    if (argc != 1)
        return -EINVAL;

    const std::string sysfs_path = findRootPort() + "/rescan";

    if (sysfs_path.empty()) {
        return -ENOENT;
    }

    std::ofstream ofile(sysfs_path);    
    
    if (!ofile.is_open()) {
        std::cout << "Failed to open " << sysfs_path << ":" << strerror(errno) << std::endl;
        return -ENOENT;
    }
   
    /* "echo 1 > /sys/bus/pci/<Root Port>/rescan" to trigger the rescan for hot plug devices */ 
    ofile << 1;
    ofile.flush();
    if (!ofile.good()) {
        std::cout << "Failed to write " << sysfs_path << ":"  << strerror(errno) << std::endl;
        ofile.close();
        return -EINVAL;
    }

    ofile.close();

    return 0;
}

static std::string findRootPort(void)
{
    std::string rootPortPath = "";
    path fPath(SYSFS_PATH);
    
    for (auto file = directory_iterator(fPath); file != directory_iterator(); file++)
    {
        rootPortPath = file->path().string();
        for (auto jfile = directory_iterator(rootPortPath); jfile != directory_iterator(); jfile++)
        {
            std::string dirName(jfile->path().string());
            if (is_directory(dirName))
            {
                std::string vendor_id, device_id;
                std::string vendorPath = dirName + "/vendor";
                
                if (boost::filesystem::exists(vendorPath))
                {
                    std::ifstream file(vendorPath);
                    std::getline(file, vendor_id);
                }
                    
                std::string devicePath = dirName + "/device";
                if (boost::filesystem::exists(devicePath))
                {
                    std::ifstream file(devicePath);
                    std::getline(file, device_id);
                }

                if (!strcmp(vendor_id.c_str(), XILINX_VENDOR) && !strcmp(device_id.c_str(), XILINX_US))
                    return rootPortPath;
            }
        }
        
        rootPortPath = "";
    }

    return rootPortPath;
}
