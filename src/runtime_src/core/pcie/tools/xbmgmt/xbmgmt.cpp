/**
 * Copyright (C) 2019 Xilinx, Inc
 * Author: Ryan Radjabi, Max Zhen, Chien-Wei Lan
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

#include "xbmgmt.h"
#include "core/pcie/linux/scan.h"

#include <unistd.h>
#include <sys/types.h>
#include <map>
#include <functional>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <climits>
#include <version.h>
#include <fstream>

struct subCmd {
    std::function<int(int, char **)> handler;
    const char *description;
    const char *usage;
    const char *expert_usage;
};

static const std::map<std::string, struct subCmd> subCmdList = {
    { "help", {helpHandler, subCmdHelpDesc, subCmdHelpUsage, nullptr} },
    { "version", {versionHandler, subCmdVersionDesc, subCmdVersionUsage, nullptr} },
    { "--version", {versionHandler, subCmdVersionDesc, subCmdVersionUsage, nullptr} },
    { "scan", {scanHandler, subCmdScanDesc, subCmdScanUsage, nullptr} },
    { "flash", {flashHandler, subCmdFlashDesc, subCmdFlashUsage, subCmdFlashExpUsage} },
    // for xbutil flash
    { "-lash", {flashXbutilFlashHandler, subCmdXbutilFlashDesc, subCmdXbutilFlashUsage, nullptr} },
    { "reset", {resetHandler, subCmdResetDesc, subCmdResetUsage, nullptr} },
    { "clock", {clockHandler, subCmdClockDesc, subCmdClockUsage, nullptr} },
    { "partition", {partHandler, subCmdPartDesc, subCmdPartUsage, subCmdPartExpUsage} },
    { "config", {configHandler, subCmdConfigDesc, subCmdConfigUsage, subCmdConfigExpUsage} },
    { "nifd", {nifdHandler, subCmdNifdDesc, subCmdNifdUsage, nullptr} },
    { "hotplug", {hotplugHandler, subCmdHotplugDesc, subCmdHotplugUsage, nullptr} },
};

const static std::vector<std::string> basic_subCmd =
    { "flash", "help", "scan", "version", "config", "partition" };

void sudoOrDie()
{
    const char* SudoMessage = "ERROR: root privileges required.";
    if ((getuid() == 0) || (geteuid() == 0))
        return;
    std::cout << SudoMessage << std::endl;
    exit(-EPERM);
}

bool canProceed()
{
    std::string input;
    bool answered = false;
    bool proceed = false;

    while (!answered) {
        std::cout << "Are you sure you wish to proceed? [y/n]: ";
        std::cin >> input;
        if(input.compare("y") == 0 || input.compare("n") == 0)
            answered = true;
    }

    proceed = (input.compare("y") == 0);
    if (!proceed)
        std::cout << "Action canceled." << std::endl;
    return proceed;
}

unsigned int bdf2index(const std::string& bdfStr)
{
    // Extract bdf from bdfStr.
    int dom = 0, b, d, f;
    char dummy;
    std::stringstream s(bdfStr);
    size_t n = std::count(bdfStr.begin(), bdfStr.end(), ':');
    if (n == 1)
        s >> std::hex >> b >> dummy >> d >> dummy >> f;
    else if (n == 2)
        s >> std::hex >> dom >> dummy >> b >> dummy >> d >> dummy >> f;
    if ((n != 1 && n != 2) || s.fail()) {
        std::cout << "ERROR: can't extract BDF from " << bdfStr << std::endl;
        return UINT_MAX;
    }

    for (unsigned i = 0; i < pcidev::get_dev_total(false); i++) {
        auto dev = pcidev::get_dev(i, false);
        if (dom == dev->domain && b == dev->bus &&
            d == dev->dev && f == dev->func) {
            return i;
        }
    }

    std::cout << "ERROR: No mgmt PF found for " << bdfStr << std::endl;
    return UINT_MAX;
}

std::string getBDF(unsigned index)
{
    char BDF[128];
    auto dev = pcidev::get_dev(index, false);
    sprintf(BDF, "%.4x:%.2x:%.2x.%.1x",
        dev->domain, dev->bus, dev->dev, dev->func);
    return std::string(BDF);
}

static inline bool isHiddenSubcmd(const std::string& cmd)
{
    return cmd[0] == '-';
}

void printHelp(bool printExpHelp)
{
    std::stringstream expert_ostr;
    std::cout << "Supported sub-commands are:" << std::endl;
    for (auto& c : subCmdList) {
        if (isHiddenSubcmd(c.first))
            continue;
        if(std::find(std::begin(basic_subCmd),
            std::end(basic_subCmd), c.first) == basic_subCmd.end()) {
            expert_ostr << "\t" << c.first << " - " << c.second.description
                << std::endl;
            continue;
        }
        std::cout << "\t" << c.first << " - " << c.second.description <<
            std::endl;
    }
    if (printExpHelp)
        std::cout << "Experts only:\n" << expert_ostr.str();
    std::cout <<
        "Run xbmgmt help <subcommand> for detailed help of each subcommand" <<
        std::endl;
}

void printSubCmdHelp(const std::string& subCmd, bool show_expert)
{
    auto cmd = subCmdList.find(subCmd);

    if (cmd == subCmdList.end()) {
        std::cout << "Unknown sub-command: " << subCmd << std::endl;
    } else {
        if (std::find(std::begin(basic_subCmd),
            std::end(basic_subCmd), subCmd) == basic_subCmd.end())
                std::cout << "Experts only sub-command, use at your own risk." << std::endl;
        if (!isHiddenSubcmd(subCmd))
            std::cout << "'" << subCmd << "' sub-command usage:" << std::endl;
        std::cout << cmd->second.usage << std::endl;
        if(show_expert)
            std::cout << cmd->second.expert_usage << std::endl;
    }
}

int xrt_xbmgmt_version_cmp() 
{
    /* Check XRT libs and driver versions. */
    std::string xrt = std::string(xrt_build_version) + "," + std::string(xrt_build_version_hash);
    if ( driver_version("xclmgmt") != "unknown" &&
        xrt.compare(driver_version("xclmgmt") ) != 0 ) {
        std::cout << "\nERROR: Mixed versions of XRT and xbmgmt are not supported. \
            \nPlease install matching versions of XRT and xbmgmt or  \
            \ndefine env variable INTERNAL_BUILD to disable this check\n" << std::endl;
        return -1;
    }
    return 0;
}

bool getenv_or_null(const char* env)
{ 
    return getenv(env) ? true : false; 
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printHelp(false);
        return -EINVAL;
    }

    std::string subCmd(argv[1]);
    auto cmd = subCmdList.find(subCmd);

    //do not proceed if xbmgmt and xrt versions don't match 
    //unless cmd is version or help or INTERNAL_BUILD is set
    if ( subCmd.find("version") == std::string::npos && subCmd.compare("help") != 0 
            && !getenv_or_null("INTERNAL_BUILD") ) { 
        if ( xrt_xbmgmt_version_cmp() != 0 )
        return -EINVAL;
    }

    if (cmd == subCmdList.end()) {
        printHelp(false);
        return -EINVAL;
    }

    --argc;
    ++argv;
    int ret = cmd->second.handler(argc, argv);
    if (ret == -EINVAL)
        printSubCmdHelp(subCmd, false);

    return ret;
}
