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
#include "xbmgmt.h"

const char *subCmdHelpDesc = "Print out help message for a sub-command";
const char *subCmdHelpUsage = "help [sub-command]";

int helpHandler(int argc, char *argv[])
{
    if (argc == 1) {
        printHelp(false);
        return 0;
    }
    std::string subCmd(argv[1]);
    if(subCmd.compare("--expert") == 0) {
        printHelp(true);
        return 0;
    }

    printSubCmdHelp(subCmd);
    return 0;
}
