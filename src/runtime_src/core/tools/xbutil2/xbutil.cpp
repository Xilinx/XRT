/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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

// Sub Commands
#include "SubCmdExamine.h"
#include "SubCmdClock.h"
#include "SubCmdDD.h"
#include "SubCmdDump.h"
#include "SubCmdDmaTest.h"
#include "SubCmdList.h"
#include "SubCmdMem.h"
#include "SubCmdM2MTest.h"
#include "SubCmdP2P.h"
#include "SubCmdProgram.h"
#include "SubCmdQuery.h"
#include "SubCmdReset.h"
#include "SubCmdScan.h"
#include "SubCmdTop.h"
#include "SubCmdVersion.h"
#include "SubCmdValidate.h"
#include "SubCmdAdvanced.h"

// Supporting tools
#include "tools/common/XBMain.h"
#include "tools/common/SubCmd.h"
#include "common/error.h"

// System include files
#include <boost/filesystem.hpp>
#include <string>
#include <iostream>
#include <exception>

// Program entry
int main( int argc, char** argv )
{
  // -- Build the supported subcommands
  SubCmdsCollection subCommands;

  {
    // Syntax: SubCmdClass( IsHidden, IsDepricated, IsPreliminary)
    subCommands.emplace_back(std::make_shared<  SubCmdExamine >(false, false, false));
    subCommands.emplace_back(std::make_shared<  SubCmdProgram >(false, false, false));
    subCommands.emplace_back(std::make_shared< SubCmdValidate >(false, false, false));
    subCommands.emplace_back(std::make_shared< SubCmdAdvanced >(false, false, true ));
    subCommands.emplace_back(std::make_shared<    SubCmdReset >(false, false, false));
  }

  // Add depricated commands
  #ifdef ENABLE_DEPRECATED_2020_1_SUBCMDS
  {
    // Syntax: SubCmdClass( IsHidden, IsDepricated, IsPreliminary)
    subCommands.emplace_back(std::make_shared<   SubCmdClock >(true, true, false));
    subCommands.emplace_back(std::make_shared<      SubCmdDD >(true, true, false));
    subCommands.emplace_back(std::make_shared<    SubCmdDump >(true, true, false));
    subCommands.emplace_back(std::make_shared< SubCmdDmaTest >(true, true, false));
    subCommands.emplace_back(std::make_shared<    SubCmdList >(true, true, false));
    subCommands.emplace_back(std::make_shared< SubCmdM2MTest >(true, true, false));
    subCommands.emplace_back(std::make_shared<     SubCmdMem >(true, true, false));
    subCommands.emplace_back(std::make_shared<     SubCmdP2P >(true, true, false));
    subCommands.emplace_back(std::make_shared<   SubCmdQuery >(true, true, false));
    subCommands.emplace_back(std::make_shared<    SubCmdScan >(true, true, false));
    subCommands.emplace_back(std::make_shared< SubCmdVersion >(true, true, false));
    subCommands.emplace_back(std::make_shared<     SubCmdTop >(true, true, false));
  }
  #endif

  // -- Determine and set the executable name for each subcommand
  boost::filesystem::path pathAndFile(argv[0]);
  const std::string executable = pathAndFile.stem().string();

  for (auto & subCommand : subCommands) {
    subCommand->setExecutableName(executable);
  }

  // -- Program Description
  const std::string description = 
  "The Xilinx (R) Board Utility (xbutil) is a standalone command line utility that"
  " is included with the Xilinx Run Time (XRT) installation package. It includes"
  " multiple commands to validate and identifythe installed card(s) along with"
  " additional card details including DDR, PCIe (R), shell name (DSA), and system"
  " information.\n\nThis information can be used for both card administration and"
  " application debugging.";

  // -- Ready to execute the code
  try {
    main_( argc, argv, description, subCommands);
    return 0;
  } catch (const std::exception &e) {
    xrt_core::send_exception_message(e.what(), executable.c_str());
  } catch (...) {
    xrt_core::send_exception_message("Unknown error", executable.c_str());
  }
  return 1;
}
