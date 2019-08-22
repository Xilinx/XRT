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
#include <functional>
#include <map>
#include <fstream>
#include <climits>
#include <getopt.h>
#include <unistd.h>

#include "flasher.h"
#include "xbmgmt.h"
#include "firmware_image.h"
#include "core/pcie/linux/scan.h"
#include "xclbin.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"

const char *subCmdPartDesc = "Show and download partition onto the device";
const char *subCmdPartUsage =
    "--program --id id [--card bdf] [--force]\n"
    "--program --path xclbin [--card bdf] [--force]\n"
    "--scan [--verbose]";

int program_prp(unsigned index, const std::string& xclbin, bool force)
{
    std::ifstream stream(xclbin.c_str(), std::ios_base::binary);

    if(!stream.is_open()) {
        std::cout << "ERROR: Cannot open " << xclbin << std::endl;
	return -ENOENT;
    }

    auto dev = pcidev::get_dev(index, false);
    int fd = dev->devfs_open("icap", O_WRONLY);

    if (fd == -1) {
        std::cout << "ERROR: Cannot open icap for writing." << std::endl;
        return -ENODEV;
    }

    stream.seekg(0, stream.end);
    int length = stream.tellg();
    stream.seekg(0, stream.beg);

    char *buffer = new char[length];
    stream.read(buffer, length);

    std::string errmsg;
    if (force)
    {
        dev->sysfs_put("", "rp_program", errmsg, "3");
        if (!errmsg.empty())
        {
            std::cout << errmsg << std::endl;
            return -EINVAL;
        }
    }

    ssize_t ret = write(fd, buffer, length);
    delete [] buffer;

    if (ret <= 0) {
        std::cout << "ERROR: Write prp to icap subdev failed." << std::endl;
        close(fd);
        return -errno;
    }
    close(fd);

    if (force)
    {
        std::cout << "CAUTION: Force downloading PRP. " <<
                "Please make sure xocl driver is unloaded." << std::endl;
        if(!canProceed())
            return -ECANCELED;

        dev->sysfs_put("", "rp_program", errmsg, "2");
        if (!errmsg.empty())
        {
            std::cout << errmsg << std::endl;
            return -EINVAL;
        }
    }
    else
    {
        dev->sysfs_put("", "rp_program", errmsg, "1");
        if (!errmsg.empty())
	{
            std::cout << errmsg << std::endl;
            return -EINVAL;
        }
    }

    return 0;
}

int program_urp(unsigned index, const std::string& xclbin)
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

#define fmt_str "    "
void scanPartitions(int index, std::vector<DSAInfo>& installedDSAs, bool verbose)
{
    Flasher f(index);
    if (!f.isValid())
        return;

    auto dev = pcidev::get_dev(index, false);
    std::vector<std::string> uuids;
    std::vector<std::string> int_uuids;
    std::string errmsg;
    dev->sysfs_get("", "logic_uuids", errmsg, uuids);
    if (!errmsg.empty() || uuids.size() == 0)
        return;

    dev->sysfs_get("", "interface_uuids", errmsg, int_uuids);
    if (!errmsg.empty() || int_uuids.size() == 0)
        return;

    DSAInfo dsa("", NULL_TIMESTAMP, uuids.back(), "");
    if (dsa.name.empty())
        return;

    std::cout << "Card [" << f.sGetDBDF() << "]" << std::endl;
    std::cout << fmt_str << "Programmable partition running on FPGA:" << std::endl;
    std::cout << fmt_str << fmt_str << dsa << std::endl;


    std::cout << fmt_str << "Programmable partitions installed in system:" << std::endl;
    if (installedDSAs.empty())
    {
        std::cout << "(None)" << std::endl;
        return;
    }

    for (auto& dsa : installedDSAs)
    {
        if (dsa.hasFlashImage || dsa.uuids.empty())
            continue;
        if (int_uuids[0].compare(dsa.uuids[1]) != 0)
            continue;
	std::cout << fmt_str << fmt_str << dsa << std::endl;
        if (dsa.uuids.size() > 2)
        {
            std::cout << fmt_str << fmt_str << fmt_str << "Interface UUID:" << std::endl;
            for (unsigned int i = 2; i < dsa.uuids.size(); i++)
            {
               std::cout << fmt_str << fmt_str << fmt_str  << dsa.uuids[i] << std::endl;
            } 
        }
    }
    std::cout << std::endl;
}

int scan(int argc, char *argv[])
{
    unsigned total = pcidev::get_dev_total(false);

    if (total == 0) {
        std::cout << "No card is found!" << std::endl;
	return 0;
    }

    bool verbose;
    const option opts[] = {
        { "verbose", no_argument, nullptr, '0' },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            verbose = true;
            break;
        default:
            return -EINVAL;
        }
    }

    auto installedDSAs = firmwareImage::getIntalledDSAs();
    for (unsigned i = 0; i < total; i++)
    {
        scanPartitions(i, installedDSAs, verbose);
    }

    return 0;
}

int program(int argc, char *argv[])
{
    if (argc < 2)
        return -EINVAL;

    unsigned index = UINT_MAX;
    bool force = false;
    std::string file, id;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "force", no_argument, nullptr, '1' },
        { "path", required_argument, nullptr, '2' },
	{ "id", required_argument, nullptr, '3' },
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
	case '3':
	    id = std::string(optarg);
	    break;
        default:
            return -EINVAL;
        }
    }

    if (!id.empty())
    {
        std::vector<DSAInfo> DSAs;

        auto installedDSAs = firmwareImage::getIntalledDSAs();
        for (DSAInfo& dsa : installedDSAs)
        {
            if (dsa.uuids.size() == 0)
                continue;
            if (!dsa.matchIntId(id))
                continue;

            DSAs.push_back(dsa);
        }
        if (DSAs.size() > 1)
	{
            std::cout << "ERROR: found duplicated partitions, please specify the entire uuid" << std::endl;
            for (DSAInfo&d : DSAs)
            {
                std::cout << d;
            }
            std::cout << std::endl;
            return -EINVAL;
        }
        else if (DSAs.size() == 0)
        {
            std::cout << "ERROR: No match partition found" << std::endl;
            return -EINVAL;
	}
	file = DSAs[0].file;
    }

    if (file.empty())
        return -EINVAL;

    if (index == UINT_MAX)
        index = 0;

    DSAInfo dsa(file);
    std::string blp_uuid;
    auto dev = pcidev::get_dev(index, false);
    std::string errmsg;

    dev->sysfs_get("", "interface_uuids", errmsg, blp_uuid);
    if (!errmsg.empty())
    {
        // 1RP platform
    	/* Get permission from user. */
        if (!force) {
            std::cout << "CAUTION: Downloading xclbin. " <<
                "Please make sure xocl driver is unloaded." << std::endl;
            if(!canProceed())
                return -ECANCELED;
        }

        std::cout << "Programming URP..." << std::endl;
        return program_urp(index, file);
    }

    for (std::string uuid : dsa.uuids)
    {
        if (blp_uuid.compare(uuid) == 0)
        {
            std::cout << "Programming PRP..." << std::endl;
            return program_prp(index, file, force);
        }
    }

    std::cout << "Programming URP..." << std::endl;
    return program_urp(index, file);
}

static const std::map<std::string, std::function<int(int, char **)>> optList = {
    { "--program", program },
    { "--scan", scan },
};

int partHandler(int argc, char *argv[])
{
    if (argc < 2)
        return -EINVAL;

    sudoOrDie();

    std::string subcmd(argv[1]);

    auto cmd = optList.find(subcmd);
    if (cmd == optList.end())
        return -EINVAL;

    argc--;
    argv++;

    return cmd->second(argc, argv);
}
