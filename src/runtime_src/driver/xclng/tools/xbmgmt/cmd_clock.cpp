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

#include "scan.h"
#include "xbmgmt.h"
#include "mgmt-ioctl.h"

const char *subCmdClockDesc = "Change various clock frequency on the device";
const char *subCmdClockUsage =
    "[--data freq] [--kernel freq] [--system freq] [--card bdf] [--force]";

int clockHandler(int argc, char *argv[])
{
    sudoOrDie();

    if (argc < 2)
        return -EINVAL;

    unsigned index = UINT_MAX;
    unsigned short data = 0;
    unsigned short kernel = 0;
    unsigned short system = 0;
    bool force = false;
    const option opts[] = {
        { "card", required_argument, nullptr, '0' },
        { "data", required_argument, nullptr, '1' },
        { "kernel", required_argument, nullptr, '2' },
        { "system", required_argument, nullptr, '3' },
        { "force", no_argument, nullptr, '4' },
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
            data = std::atoi(optarg);
            if (data == 0)
                return -EINVAL;
            break;
        case '2':
            kernel = std::atoi(optarg);
            if (kernel == 0)
                return -EINVAL;
            break;
        case '3':
            system = std::atoi(optarg);
            if (system == 0)
                return -EINVAL;
            break;
        case '4':
            force = true;
            break;
        default:
            return -EINVAL;
        }
    }

    /* Should specify at least one freq. */
    if (!data && !kernel && !system)
        return -EINVAL;

    if (index == UINT_MAX)
        index = 0;

    /* Get permission from user. */
    if (!force) {
        std::cout << "CAUTION: Changing clock frequency. " <<
            "Please make sure xocl driver is unloaded." << std::endl;
        if(!canProceed())
            return -ECANCELED;
    }

    xclmgmt_ioc_freqscaling obj = { 0 };
    obj.ocl_target_freq[DATA_CLK] = data;
    obj.ocl_target_freq[KERNEL_CLK] = kernel;
    obj.ocl_target_freq[SYSTEM_CLK] = system;
    auto dev = pcidev::get_dev(index, false);
    int ret = dev->ioctl(XCLMGMT_IOCFREQSCALE, &obj);

    return ret ? -errno : ret;
}
