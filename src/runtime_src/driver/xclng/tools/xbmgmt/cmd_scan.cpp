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

#include "scan.h"
#include "xbmgmt.h"

const char *subCmdScanDesc = "List all detected mgmt PCIE functions";
const char *subCmdScanUsage = "(no options supported)";

int scanHandler(int argc, char *argv[])
{
    if (argc != 1)
        return -EINVAL;

    size_t total = pcidev::get_dev_total(false);
    
    if (total == 0) {
        std::cout << "No card is found!" << std::endl;
    } else {
        for (size_t i = 0; i < total; i++)
            std::cout << pcidev::get_dev(i, false) << std::endl;
    }

    return 0;
}
