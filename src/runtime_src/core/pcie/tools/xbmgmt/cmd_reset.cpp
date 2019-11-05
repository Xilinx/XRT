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
#include <climits>
#include <getopt.h>

#include "xbmgmt.h"
#include "core/pcie/linux/scan.h"
#include "xclbin.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"

const char *subCmdResetDesc = "Perform various flavors of reset on the device";
const char *subCmdResetUsage = "--hot | --kernel | --ecc [--card bdf] [--force]";

static int resetEcc(std::shared_ptr<pcidev::pci_device> dev)
{
    std::string errmsg;
    std::vector<char> buf;

    dev->sysfs_get("icap", "mem_topology", errmsg, buf);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }

    const mem_topology *map = (mem_topology *)buf.data();
    if(buf.empty() || map->m_count == 0) {
        std::cout << "WARNING: 'mem_topology' not found, "
            << "unable to query ECC info. Has the xclbin been loaded? "
            << "See 'xbmgmt program'." << std::endl;
        return -ENOENT;
    }

    for(int32_t i = 0; i < map->m_count; i++) {
        if(!map->m_mem_data[i].m_used)
            continue;
        dev->sysfs_put(
            reinterpret_cast<const char *>(map->m_mem_data[i].m_tag),
            "ecc_reset", errmsg, "1");
    }

    return 0;
}

int resetHandler(int argc, char *argv[])
{
    sudoOrDie();

    if (argc < 2)
        return -EINVAL;

    unsigned index = UINT_MAX;
    int hot = 0;
    int kernel = 0;
    int ecc = 0;
    bool force = false;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "hot", no_argument, nullptr, '1' },
        { "kernel", no_argument, nullptr, '2' },
        { "ecc", no_argument, nullptr, '3' },
        { "force", no_argument, nullptr, '4' },
        { nullptr, 0, nullptr, 0 },
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
            hot = 1;
            break;
        case '2':
            kernel = 1;
            break;
        case '3':
            ecc = 1;
            break;
        case '4':
            force = true;
            break;
        default:
            return -EINVAL;
        }
    }

    /* Can't do multiple reset in one shot. */
    if (hot + kernel + ecc != 1)
        return -EINVAL;

    if (index == UINT_MAX)
        index = 0;

    /* Get permission from user. */
    if (!force) {
        if (hot) {
            std::cout << "CAUTION: Performing hot reset. " <<
                "Please make sure xocl driver is unloaded." << std::endl;
        } else if (kernel) {
            std::cout << "CAUTION: Performing PR region reset. " <<
                "Please make sure no application is currently running." <<
                std::endl;
        } else if (ecc) {
            std::cout << "CAUTION: resetting all ECC counters. " << std::endl;
        }
        if(!canProceed())
            return -ECANCELED;
    }

    int ret = 0;
    auto dev = pcidev::get_dev(index, false);
    int fd = dev->open("", O_RDWR);
    if (hot) {
        ret = dev->ioctl(fd, XCLMGMT_IOCHOTRESET);
        if (ret == 0)
            std::cout << "Successfully reset Card[" << getBDF(index)
                      << "]"<< std::endl;
	ret = ret ? -errno : ret;
    } else if (kernel) {
        ret = dev->ioctl(fd, XCLMGMT_IOCOCLRESET);
	ret = ret ? -errno : ret;
    } else if (ecc) {
        ret = resetEcc(dev);
    }
    dev->close(fd);

    return ret;
}
