// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// Sub Commands
#include "SubCmdAdvanced.h"
#include "SubCmdConfigure.h"
#include "SubCmdDump.h"
#include "SubCmdExamine.h"
#include "SubCmdProgram.h"
#include "SubCmdReset.h"

// Supporting tools
#include "common/error.h"
#include "tools/common/SubCmd.h"
#include "tools/common/XBMain.h"
#include "tools/common/XBUtilities.h"

#include "xrt.h"

// System include files
#include <boost/filesystem.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <iostream>
#include <exception>

const std::string& command_config = 
R"(
[{
    "name": "cmd_configs",
    "contents": [{
        "name": "common",
        "contents": [{
            "name": "examine",
            "contents": ["cmc", "firewall", "host", "mailbox", "mechanical", "platform", "vmr"]
        },{
            "name": "configure",
            "contents": ["input", "retention"]
        }]
    }]
}]
)";

// Program entry
int main( int argc, char** argv )
{
  // -- Build the supported subcommands
  SubCmdsCollection subCommands;

  boost::property_tree::ptree configTree;
  std::istringstream command_config_stream(command_config);
  boost::property_tree::read_json(command_config_stream, configTree);

  {
    // Syntax: SubCmdClass( IsHidden, IsDepricated, IsPreliminary)
    subCommands.emplace_back(std::make_shared<   SubCmdProgram  >(false, false, false));
    subCommands.emplace_back(std::make_shared<     SubCmdReset  >(false, false, false));
    subCommands.emplace_back(std::make_shared<  SubCmdAdvanced  >(false, false,  true));
    subCommands.emplace_back(std::make_shared<   SubCmdExamine  >(false, false, false, configTree));
    subCommands.emplace_back(std::make_shared<      SubCmdDump  >(false, false, false));
    subCommands.emplace_back(std::make_shared< SubCmdConfigure  >(false, false, false, configTree));
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
    main_( argc, argv, executable, description, subCommands);
    return 0;
  } catch (const xrt_core::error& e) {
    // Clean exception exit
    // If the exception is "operation_canceled" then don't print the header debug info
    if (e.code().value() != static_cast<int>(std::errc::operation_canceled))
      xrt_core::send_exception_message(e.what(), executable.c_str());
    else
      XBUtilities::print_exception(e);
  } catch (const std::exception &e) {
    xrt_core::send_exception_message(e.what(), executable.c_str());
  } catch (...) {
    xrt_core::send_exception_message("Unknown error", executable.c_str());
  }
  return 1;
}
