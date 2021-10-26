/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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

//
// This utility is implemented by porting flash code from xbmgmt. Since that
// it's used only by non-XRT users, we do not expect to maintain and enhance
// this code a lot in the future. Hence, no cleanup effort is ever attempted
// while porting the code from xbmgmt at this point.
// If it works, don't change it.
//

#include "xbflash2.h"

#include <iostream>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <getopt.h>
#include <libgen.h>
#include <unistd.h>

struct subCmd {
    std::function<int(po::variables_map)> handler;
    const char *description;
    const char *usage;
};

static const std::map<std::string, struct subCmd> subCmdList = {
    { "help", {helpHandler, subCmdHelpDesc, subCmdHelpUsage} },
    { "program", {programHandler, subCmdProgramDesc, subCmdProgramUsage} },
    { "dump", {dumpHandler, subCmdDumpDesc, subCmdDumpUsage} },
};

const static std::vector<std::string> basic_subCmd =
    { "help", "program", "dump" };

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
      answered = (input.compare("y") == 0 || input.compare("n") == 0);
    }

    proceed = (input.compare("y") == 0);
    if (!proceed)
      std::cout << "Action canceled." << std::endl;
    return proceed;
}

void printSubCmdHelp(const std::string& subCmd)
{
    auto cmd = subCmdList.find(subCmd);

    if (cmd == subCmdList.end()) {
      std::cout << "Unknown sub-command: " << subCmd << std::endl;
    } else {
      if (std::find(std::begin(basic_subCmd),
          std::end(basic_subCmd), subCmd) == basic_subCmd.end())
              std::cout << "Experts only sub-command, use at your own risk." << std::endl;
      std::cout << "'" << subCmd << "' command" << std::endl;
      std::cout << "DESCRIPTION: " << cmd->second.description << std::endl;
      std::cout << "USAGE:\n" << cmd->second.usage << std::endl;
    }
}

void printHelp(bool printExpHelp)
{
    std::stringstream expert_ostr;

    std::cout << "DESCRIPTIONS: utility is available as a way to flash a custom image onto given device.\n" << std::endl;
    std::cout << "USAGE: xbflash2 [--help] [command [commandArgs]] [-d arg] [--version] [--verbose] [--batch] [--force]\n" << std::endl;
    std::cout << "AVAILABLE COMMANDS:" << std::endl;
    for (auto& c : subCmdList) {
      if(std::find(std::begin(basic_subCmd), std::end(basic_subCmd), c.first) == basic_subCmd.end()) {
        expert_ostr << "\t" << c.first << " - " << c.second.description << std::endl;
            continue;
      }
      std::cout << "\t" << c.first << " - " << c.second.description << std::endl;
    }

    if (printExpHelp)
      std::cout << "Experts only:\n" << expert_ostr.str();

    std::cout <<
      "Run xbflash2 help <subcommand> for detailed help of each subcommand" <<
      std::endl;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printHelp(false);
        return -EINVAL;
    }

    std::string subCmd(argv[1]);
    auto cmd = subCmdList.find(subCmd);

    if (cmd == subCmdList.end()) {
        printHelp(false);
        return -EINVAL;
    }

    if (argc < 3) {
      printSubCmdHelp(subCmd);
        return -EINVAL;
    }

    po::options_description description("Usage");

    description.add_options()
    ("device",    po::value<std::string>(), "device in BDF format\n")
    ("flash",      po::value<std::string>(), "Flash type - spi | qspips")
    ("flash-part",      po::value<std::string>(), "Flash Part")
    ("revert-to-golden", "Revert to Golden Image")
    ("erase", "Erase flash on device")
    ("image", po::value<std::vector<std::string>>(), "Specifies an image to use used to update the persistent device.  Value values:\n"
                                                                    "  Name (and path) to the mcs image on disk\n"
                                                                    "  Name (and path) to the xsabin image on disk")
    ("dual-flash", "Dual Flash")
    ("force", "When possible, force an operation")
    ("bar", po::value<std::string>(), "bar")
    ("bar-offset", po::value<std::string>(), "bar-offset")
    ("offset", po::value<std::string>(), "offset-on-flash-to-start-with")
    ("output", po::value<std::string>(), "output-file-to-save-read-contents")
    ("length", po::value<std::string>(), "length-to-read")
    ("help", "Display this help message");

    // -- Parse the command line
    po::parsed_options parsed = po::command_line_parser(argc, argv).options(description).run(); // Parse the options

    po::variables_map vm;

    try {
      po::store(parsed, vm);          // Can throw
      po::notify(vm);                 // Can throw
    } catch (po::error& e) {
      // Something bad happen with parsing our options
      printHelp(false);
    }

    int ret = cmd->second.handler(vm);
    if (ret == -EINVAL)
      printSubCmdHelp(subCmd);

    return ret;
}
