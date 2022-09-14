// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// Sub Commands
#include "SubCmdAdvanced.h"
#include "SubCmdConfigure.h"
#include "SubCmdExamine.h"
#include "SubCmdProgram.h"
#include "SubCmdReset.h"
#include "SubCmdValidate.h"

// Supporting tools
#include "common/error.h"
#include "tools/common/SubCmd.h"
#include "tools/common/SubCmdJSON.h"
#include "tools/common/XBMain.h"
#include "tools/common/XBUtilities.h"

// System include files
#include <exception>
#include <iostream>
#include <string>

#include <boost/filesystem.hpp>

// Program entry
int main( int argc, char** argv )
{
  // -- Build the supported subcommands
  SubCmdsCollection subCommands;
  const std::string executable = "xbutil";

  {
    // Syntax: SubCmdClass( IsHidden, IsDepricated, IsPreliminary)
    subCommands.emplace_back(std::make_shared<  SubCmdExamine  >(false, false, false));
    subCommands.emplace_back(std::make_shared<  SubCmdProgram  >(false, false, false));
    subCommands.emplace_back(std::make_shared<    SubCmdReset  >(false,  false, false));
    subCommands.emplace_back(std::make_shared< SubCmdConfigure >(false,  false, false));

    // Parse sub commands from json files
    populateSubCommandsFromJSON(subCommands, executable);

#ifdef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
    subCommands.emplace_back(std::make_shared< SubCmdValidate >(false,  false, false));
#endif

    subCommands.emplace_back(std::make_shared< SubCmdAdvanced >(true,  false, true ));
  }

  for (auto & subCommand : subCommands) {
    subCommand->setExecutableName(executable);
  }

  // -- Program Description
  const std::string description = 
  "The Xilinx (R) Board Utility (xbutil) is a standalone command line utility that"
  " is included with the Xilinx Run Time (XRT) installation package. It includes"
  " multiple commands to validate and identify the installed card(s) along with"
  " additional card details including DDR, PCIe (R), shell name (DSA), and system"
  " information.\n\nThis information can be used for both card administration and"
  " application debugging.";

  // -- Ready to execute the code
  try {
    main_( argc, argv, executable, description, subCommands);
    return 0;
  } catch (const xrt_core::error& e) {
    // Clean exception exit
    // If the exception is "operation_canceled" then don't print the header debug info
    if (e.code().value() != static_cast<int>(std::errc::operation_canceled))
      xrt_core::send_exception_message(e.what(), executable.c_str());
    else // Handle the operation canceled case
      XBUtilities::print_exception(e);
  } catch (const std::exception &e) {
    xrt_core::send_exception_message(e.what(), executable.c_str());
  } catch (...) {
    xrt_core::send_exception_message("Unknown error", executable.c_str());
  }

  return 1;
}
