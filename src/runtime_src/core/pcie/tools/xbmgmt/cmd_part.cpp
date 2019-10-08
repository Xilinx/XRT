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
#include <libgen.h>
#include <stdint.h>
#include <dirent.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "boost/filesystem.hpp"
#include "flasher.h"
#include "xbmgmt.h"
#include "firmware_image.h"
#include "core/pcie/linux/scan.h"
#include "xclbin.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"

using namespace boost::filesystem;

const char *subCmdPartDesc = "Show and download partition onto the device";
const char *subCmdPartUsage =
    "--program --name name [--id interface-uuid] [--card bdf] [--force]\n"
    "--program --path xclbin [--card bdf] [--force]\n"
    "--scan [--verbose]";

#define indent(level)	std::string((level) * 4, ' ')
int program_prp(unsigned index, const std::string& xclbin, bool force)
{
    std::ifstream stream(xclbin.c_str(), std::ios_base::binary);

    if(!stream.is_open()) {
        std::cout << "ERROR: Cannot open " << xclbin << std::endl;
	return -ENOENT;
    }

    auto dev = pcidev::get_dev(index, false);
    int fd = dev->open("icap", O_WRONLY);

    if (fd == -1) {
        std::cout << "ERROR: Cannot open icap for writing." << std::endl;
        return -ENODEV;
    }

    stream.seekg(0, stream.end);
    int length = stream.tellg();
    stream.seekg(0, stream.beg);

    std::unique_ptr<char> buffer(new char[length]);
    stream.read(buffer.get(), length);

    std::string errmsg;
    if (force)
    {
        dev->sysfs_put("", "rp_program", errmsg, "3");
        if (!errmsg.empty())
        {
            std::cout << errmsg << std::endl;
            dev->close(fd);
            return -EINVAL;
        }
    }

    ssize_t ret = write(fd, buffer.get(), length);

    if (ret <= 0) {
        std::cout << "ERROR: Write prp to icap subdev failed." << std::endl;
        dev->close(fd);
        return -errno;
    }
    dev->close(fd);

    if (force)
    {
        std::cout << "CAUTION! Force downloading PRP inappropriately may hang the host. Please make sure xocl driver is unloaded or detached from the corresponding board. The host will hang with attached xocl driver instance." << std::endl;
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
    std::cout << "Program successfully" << std::endl;

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
    int fd = dev->open("", O_RDWR);
    int ret = dev->ioctl(fd, XCLMGMT_IOCICAPDOWNLOAD_AXLF, &obj);
    dev->close(fd);
    delete [] buffer;

    return ret ? -errno : ret;
}

void printTree (boost::property_tree::ptree &pt, int level) {
    if (pt.empty()) {
        std::cout << ": " << pt.data() << std::endl;
    } else {
        if (level > 1)
            std::cout << std::endl;
        for (auto pos = pt.begin(); pos != pt.end();) {
            std::cout << indent(level+1) << pos->first;
            printTree(pos->second, level + 1);
            ++pos;
        }
    }
    return;
}

void printPartinfo(int index, DSAInfo& d, unsigned int level)
{
    auto dev = pcidev::get_dev(index, false);
    std::vector<std::string> partinfo;

    dev->get_partinfo(partinfo, d.dtbbuf.get());
    if ((level > 0 && partinfo.size() <= level) || partinfo.empty())
        return;

    auto info = partinfo[level];
    if (info.empty())
        return;
    boost::property_tree::ptree ptInfo;
    std::istringstream is(info);
    boost::property_tree::read_json(is, ptInfo);
    std::cout << indent(3) << "Partition info";
    printTree(ptInfo, 3);
}

void scanPartitions(int index, bool verbose)
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

    DSAInfo d("", NULL_TIMESTAMP, uuids[0], "");
    if (d.name.empty())
        return;

    std::cout << "Card [" << f.sGetDBDF() << "]" << std::endl;
    std::cout << indent(1) << "Partitions running on FPGA:" << std::endl;
    for (unsigned int i = 0; i < uuids.size(); i++)
    {
        DSAInfo d("", NULL_TIMESTAMP, uuids[i], "");
        std::cout << indent(2) << d.name << std::endl;
        std::cout << indent(3) << "logic-uuid:" << std::endl;
        std::cout << indent(3)  << uuids[i] << std::endl;
        std::cout << indent(3) << "interface-uuid:" << std::endl;
        std::cout << indent(3)  << int_uuids[i] << std::endl;
        if (verbose)
            printPartinfo(index, d, i);
    }

    auto installedDSAs = firmwareImage::getIntalledDSAs();
    std::cout << indent(1) << "Partitions installed in system:" << std::endl;
    if (installedDSAs.empty())
    {
        std::cout << "(None)" << std::endl;
        return;
    }

    for (auto& dsa : installedDSAs)
    {
        unsigned int i;
        if (dsa.hasFlashImage || dsa.uuids.empty())
            continue;
	for (i = 0; i < dsa.uuids.size(); i++)
        {
            if (int_uuids[0].compare(dsa.uuids[i]) == 0)
                break;
        }
	if (i == dsa.uuids.size())
            continue;	
	dsa.uuids.erase(dsa.uuids.begin()+i);
	std::cout << indent(2) << dsa.name << std::endl;
        if (dsa.uuids.size() > 1)
        {
            std::cout << indent(3) << "logic-uuid:" << std::endl;
            std::cout << indent(3)  << dsa.uuids[0] << std::endl;
            std::cout << indent(3) << "interface-uuid:" << std::endl;
            for (i = 1; i < dsa.uuids.size(); i++)
            {
               std::cout << indent(3) << dsa.uuids[i] << std::endl;
            } 
        }
        if (verbose)
            printPartinfo(index, dsa, 0);
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

    bool verbose = false;
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

    for (unsigned i = 0; i < total; i++)
    {
        scanPartitions(i, verbose);
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
    std::string plp;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "force", no_argument, nullptr, '1' },
        { "path", required_argument, nullptr, '2' },
        { "id", required_argument, nullptr, '3' },
        { "name", required_argument, nullptr, '4' },
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
        case '4':
            plp = std::string(optarg);
            break;
        default:
            return -EINVAL;
        }
    }

    if (index == UINT_MAX)
        index = 0;

    Flasher f(index);
    if (!f.isValid())
        return -EINVAL;

    std::string blp_uuid, logic_uuid;
    auto dev = pcidev::get_dev(index, false);
    std::string errmsg;

    dev->sysfs_get("rom", "uuid", errmsg, logic_uuid);
    if (!errmsg.empty() || logic_uuid.empty())
    {
        // 1RP platform
    	/* Get permission from user. */
        if (!force) {
            std::cout << "CAUTION: Downloading xclbin. " <<
                "Please make sure xocl driver is unloaded." << std::endl;
            if(!canProceed())
                return -ECANCELED;
        }

        std::cout << "Programming ULP on Card [" << f.sGetDBDF() << "]..." << std::endl;
        return program_urp(index, file);
    }

    dev->sysfs_get("", "interface_uuids", errmsg, blp_uuid);
    if (!errmsg.empty() || blp_uuid.empty())
    {
        std::cout << "ERROR: Can not get BLP interface uuid. Please make sure corresponding BLP package is installed." << std::endl;
	return -EINVAL;
    }
    if (file.empty())
    {
        std::vector<DSAInfo> DSAs;

        auto installedDSAs = firmwareImage::getIntalledDSAs();
        for (DSAInfo& dsa : installedDSAs)
        {
            if (dsa.uuids.size() == 0)
                continue;

            if (!id.empty() && !dsa.matchIntId(id))
                continue;

            if (!plp.empty() && dsa.name.compare(plp))
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
    else
    {
        DIR *dp;
        dp = opendir(file.c_str());
	if (dp)
        {
            path formatted_fw_dir(file);
            for (recursive_directory_iterator iter(formatted_fw_dir, symlink_option::recurse), end;
                iter != end;
            )
            {
                DSAInfo d(iter->path().string());
                if (d.uuids.size() > 0)
                {
                    file = iter->path().string();
                    break;
                }
                ++iter;
            }
            closedir(dp);
        }
    }

    if (file.empty())
    {
        std::cout << "ERROR: can not find partition file" << std::endl;
    }

    DSAInfo dsa(file);
    if (dsa.uuids.size() == 0)
    {
        std::cout << "Programming ULP on Card [" << f.sGetDBDF() << "]..." << std::endl;
        return program_urp(index, file);
    }

    std::cout << "Programming PLP on Card [" << f.sGetDBDF() << "]..." << std::endl;
    std::cout << "Partition file: " << dsa.file << std::endl;
    for (std::string uuid : dsa.uuids)
    {
        if (blp_uuid.compare(uuid) == 0)
        {
            return program_prp(index, file, force);
        }
    }

    std::cout << "ERROR: uuid does not match BLP" << std::endl;
    return -EINVAL;
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
