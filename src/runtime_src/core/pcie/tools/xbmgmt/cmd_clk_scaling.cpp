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
#include <cstring>
#include <climits>
#include <iostream>
#include <getopt.h>

#include "core/pcie/linux/scan.h"
#include "xbmgmt.h"

const char *subCmdClkScalingDesc = "Clock scaling feature configuration";
const char *subCmdClkScalingUsage =
    "[-status]\n"
    "[-card bdf]\n"
    "[-set_target_power numeric]\n"
    "[-set_target_temp numeric]\n"
    "[-set_governor power|temp]\n"
    "[-scaling_force_en 1|0]\n";

static int getClkScalingStatus(std::shared_ptr<pcidev::pci_device> dev)
{
    std::string errmsg;
    std::string mode;
    int enabled = 0;
    int target = 0;

    dev->sysfs_get("xmc", "scaling_enabled", errmsg, enabled);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }

    if (enabled)
        std::cout << "clock scaling feature is enabled" << std::endl;
    else
        std::cout << "clock scaling feature is not enabled" << std::endl;

    dev->sysfs_get("xmc", "scaling_target_power", errmsg, target);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }
    std::cout << "Target power: " << target << " Watt" << std::endl;

    dev->sysfs_get("xmc", "scaling_target_temp", errmsg, target);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }
    std::cout << "Target temperature: " << target << " degree Celcius\n";

    dev->sysfs_get("xmc", "scaling_threshold_power", errmsg, target);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }
    std::cout << "Threshold power: " << target << " Watt" << std::endl;

    dev->sysfs_get("xmc", "scaling_threshold_temp", errmsg, target);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }
    std::cout << "Threshold temperature: " << target << " degree Celcius\n";

    dev->sysfs_get("xmc", "scaling_governor", errmsg, mode);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }
    std::cout << "clock scaling governor mode: " << mode << std::endl;

    return 0;
}

static int setTargetPower(std::shared_ptr<pcidev::pci_device> dev, int target)
{
    std::string errmsg;

    dev->sysfs_put("xmc", "scaling_target_power", errmsg,
		   std::to_string(target));
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }

    return 0;
}

static int setTargetTemp(std::shared_ptr<pcidev::pci_device> dev, int target)
{
    std::string errmsg;

    dev->sysfs_put("xmc", "scaling_target_temp", errmsg,
		   std::to_string(target));
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }

    return 0;
}

static int setClkScalingGovernor(std::shared_ptr<pcidev::pci_device> dev,
				 std::string mode)
{
    std::string errmsg;

    dev->sysfs_put("xmc", "scaling_governor", errmsg, mode);
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }

    return 0;
}

static int forceClkScalingEnable(std::shared_ptr<pcidev::pci_device> dev,
				 int cs_enable)
{
    std::string errmsg;

    dev->sysfs_put("xmc", "scaling_force_en", errmsg,
		   std::to_string(cs_enable));
    if (!errmsg.empty()) {
        std::cout << errmsg << std::endl;
        return -EINVAL;
    }

    return 0;
}

int clockScalingHandler(int argc, char *argv[])
{
    if (argc < 1)
        return -EINVAL;

    unsigned index = UINT_MAX;
    int status = 0;
    int target_power = 0, target_temp = 0;
    int set_mode = 0, force_enable = 0, cs_enable = 0;
    std::string mode;

    const option opts[] = {
        { "status", no_argument, nullptr, '0' },
        { "card", required_argument, nullptr, '1' },
        { "set_target_power", required_argument, nullptr, '2' },
        { "set_target_temp", required_argument, nullptr, '3' },
        { "set_governor", required_argument, nullptr, '4' },
        { "scaling_force_en", required_argument, nullptr, '5' },
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, "", opts, nullptr);
        if (opt == -1)
            break;

        switch (opt) {
        case '0':
            status = 1;
            break;
        case '1':
            index = bdf2index(optarg);
            if (index == UINT_MAX)
                return -ENOENT;
            break;
        case '2':
	    sudoOrDie();
            target_power = bdf2index(optarg);
            break;
        case '3':
	    sudoOrDie();
            target_temp = bdf2index(optarg);
            break;
        case '4':
	    sudoOrDie();
	    set_mode = 1;
            mode = std::string(optarg);
            break;
        case '5':
	    sudoOrDie();
	    force_enable = 1;
            cs_enable = bdf2index(optarg);
            break;
        default:
            return -EINVAL;
        }
    }

    if (index == UINT_MAX)
        index = 0;

    int ret = 0;
    auto dev = pcidev::get_dev(index, false);

    if (status)
        ret = getClkScalingStatus(dev);
    else if (target_power)
        ret = setTargetPower(dev, target_power);
    else if (target_power)
        ret = setTargetTemp(dev, target_temp);
    else if (set_mode)
        ret = setClkScalingGovernor(dev, mode);
    else if (force_enable)
        ret = forceClkScalingEnable(dev, cs_enable);

    return ret;
}
