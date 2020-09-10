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
#include "SubCmdProgram.h"
#include "SubCmdReset.h"
#include "SubCmdStatus.h"
#include "SubCmdAdvanced.h"

// Supporting tools
#include "tools/common/XBMain.h"
#include "tools/common/SubCmd.h"
#include "common/error.h"

#include "xrt.h"

// System include files
#include <boost/filesystem.hpp>
#include <string>
#include <iostream>
#include <exception>

// Program entry
int main( int argc, char** argv )
{
  // force linking with libxrt_core
  // find a way in CMake to specify -undef <symbol>
  try{
    xclProbe();
  } catch (...) {
    xrt_core::send_exception_message("xclProbe failed");
  }

  // -- Build the supported subcommands
  SubCmdsCollection subCommands;

  {
    // Syntax: SubCmdClass( IsHidden, IsDepricated, IsPreliminary)
    subCommands.emplace_back(std::make_shared<   SubCmdProgram  >(false, false, false));
    subCommands.emplace_back(std::make_shared<     SubCmdReset  >(false, false, false));
    subCommands.emplace_back(std::make_shared<  SubCmdAdvanced  >(true,  false,  true));
    subCommands.emplace_back(std::make_shared<    SubCmdStatus  >(false, false, false));
  }

  const std::string executable = "xbmgmt";

  for (auto & subCommand : subCommands) 
    subCommand->setExecutableName(executable);

  // -- Program Description
  const std::string description = 
  "The Xilinx (R) Board Management (xbmgmt) is a standalone command line utility that"
  " is included with the Xilinx Run Time (XRT) installation package.";

  // -- Ready to execute the code
  try {
    main_(argc, argv, executable, description, subCommands);
    return 0;
  } catch (const std::exception &e) {
    xrt_core::send_exception_message(e.what(), executable.c_str());
  } catch (...) {
    xrt_core::send_exception_message("Unknown error", executable.c_str());
  }
  return 1;
}
