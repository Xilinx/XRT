/**
 * Copyright (C) 2022 Xilinx, Inc
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
#include "SubCmdDump.h"

// Supporting tools
#include "tools/common/SubCmd.h"
#include "XBFMain.h"

// System include files
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

// Program entry
int main( int argc, char** argv )
{
  // -- Build the supported subcommands
  SubCmdsCollection subCommands;
  const std::string executable = "xbflash2";

  {
    // Syntax: SubCmdClass( IsHidden, IsDepricated, IsPreliminary)
    subCommands.emplace_back(std::make_shared<  SubCmdProgram  >(false, false, false));
    subCommands.emplace_back(std::make_shared<  SubCmdDump  >(false,  false, false));
  }

  for (auto & subCommand : subCommands)
      subCommand->setExecutableName(executable);

  // -- Program Description
  const std::string description =
      "The Xilinx (R) Board Flash utility (xbflash2) is a standalone command line utility  to flash a custom image onto given device.";

  // -- Ready to execute the code
  try {
    main_( argc, argv, executable, description, subCommands);
    return 0;
  } catch (const std::exception &e) {
      std::cerr << "ERROR: " << e.what() << std::endl;
  }  catch (...) {
      std::cerr << "Unknown Error :  " << executable << std::endl;
  }
  return 1;
}
