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

#define SYSFS_PATH      "/sys/bus/pci/devices"
#define XILINX_VENDOR   "0x10ee"
#define XILINX_US       "0x9134"
#define POLL_TIMEOUT    60      /* Set a pool timeout as 60sec */

static int hotplugRescan(void);
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
        /* Rescan from /sys/bus/pci/<Root Port>/rescan */
        ret = hotplugRescan();
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

static bool check_mgmt_present(boost::filesystem::path rootPath)
{
    boost::filesystem::path fileDir(rootPath);
    boost::filesystem::path vendorPath, devicePath;

    for (boost::filesystem::recursive_directory_iterator iter(fileDir, boost::filesystem::symlink_option::recurse), end;
            iter != end;)
    {
        std::string name = iter->path().string();
        if (is_symlink(iter->path()))
            iter.no_push();

        if (iter->path().filename() == "mgmt_pf")
            return true;

        ++iter;
    }

    return false;
}


static std::list<std::string> findXilinxRootPort(boost::filesystem::path sysfsPath)
{
    std::list<std::string> rootportList;

    for (auto file = boost::filesystem::directory_iterator(sysfsPath);
            file != boost::filesystem::directory_iterator(); file++)
    {
        boost::filesystem::path iPath = file->path();
        for (auto jfile = boost::filesystem::directory_iterator(iPath);
                jfile != boost::filesystem::directory_iterator(); jfile++)
        {
            boost::filesystem::path dirName = jfile->path();
            if (!boost::filesystem::is_directory(dirName))
                continue;

            std::string vendor_id, device_id;

            try
            {
                boost::filesystem::path vendorPath = dirName;
                vendorPath /= "vendor";
                if (boost::filesystem::exists(vendorPath))
                {
                    boost::filesystem::ifstream file(vendorPath);
                    file >> vendor_id;
                    file.close();
                }

                boost::filesystem::path devicePath = dirName;
                devicePath /= "device";
                if (boost::filesystem::exists(devicePath))
                {
                    boost::filesystem::ifstream file(devicePath);
                    file >> device_id;
                    file.close();
                }
            }
            catch (const boost::filesystem::ifstream::failure& e)
            {
                continue;
            }

            if (!vendor_id.compare(XILINX_VENDOR) && !device_id.compare(XILINX_US)) 
                if (!check_mgmt_present(iPath)) 
                    rootportList.push_back(iPath.string());
        }
    }

    return rootportList;
}

static int hotplugRescan(void)
{
    boost::filesystem::ofstream ofile;
    /* Find all the RootPorts where Xilinx devices are connected */
    std::list<std::string> rootportList = findXilinxRootPort(SYSFS_PATH);

    for (std::list<std::string>::iterator it = rootportList.begin(); it != rootportList.end(); ++it)
    {
        std::string iPath = *it;
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
    }
    
    return 0;
}
