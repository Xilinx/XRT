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
#include <version.h>
#include <fstream>
#include "xbmgmt.h"

const char *subCmdVersionDesc = "Print out xrt build version";
const char *subCmdVersionUsage = "(no options supported)";

static std::string driver_version(std::string driver)
{
    std::string line("unknown");
    std::string path("/sys/bus/pci/drivers/");
    path += driver;
    path += "/module/version";
    std::ifstream ver(path);
    if (ver.is_open())
        getline(ver, line);
    return line;
}

int versionHandler(int argc, char *argv[])
{
    if (argc != 1)
        return -EINVAL;

    xrt::version::print(std::cout);
    std::cout.width(26); std::cout << std::internal << "XOCL: " << driver_version("xocl") << std:: endl;
    std::cout.width(26); std::cout << std::internal << "XCLMGMT: " << driver_version("xclmgmt") << std::endl;    
    return 0;
}
