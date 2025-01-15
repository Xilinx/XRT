// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

// Sub Commands
#include "SubCmdAdvanced.h"
#include "SubCmdConfigure.h"
#include "SubCmdExamine.h"
#include "SubCmdProgram.h"
#include "SubCmdReset.h"
#include "SubCmdValidate.h"
#include "tools/common/tests/TestValidateUtilities.h"

// Supporting tools
#include "common/error.h"
#include "tools/common/SubCmd.h"
#include "tools/common/SubCmdJSON.h"
#include "tools/common/XBMain.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/JSONConfigurable.h"
#include "core/common/module_loader.h"

// System include files
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

const std::string& command_config = 
R"(
[{
  "alveo": [{
    "examine": [{
      "report": ["dynamic-regions", "electrical", "host", "mechanical", "memory", "pcie-info", "platform", "thermal", "error", "firewall", "mailbox", "debug-ip-status", "qspi-status", "aie", "aiemem", "aieshim"]
    }]
  },{
    "configure": [{
      "suboption": ["host-mem", "p2p"]
    }]
  },{
    "advanced":[{
      "suboption": ["read-mem", "write-mem"]
    }]
  },{
    "validate": [{
      "test": ["aux-connection", "pcie-link", "sc-version", "verify", "dma", "mem-bw", "p2p", "m2m", "hostmem-bw", "aie", "ps-aie", "ps-pl-verify", "ps-verify", "ps-iops"]
    }]
  },{
    "reset": [{}]
  },{
    "program": [{}]
  }]
},{
  "aie": [{
    "examine": [{
      "report": ["host", "platform", "aie-partitions", "telemetry"]
    }]
  },{
    "configure": [{
      "suboption": ["pmode", "force-preemption"]
    }]
  },{
    "advanced":[{
      "suboption": ["read-aie-reg", "aie-clock", "report"]
    }]
  },{
    "validate": [{
      "test": ["latency", "throughput", "cmd-chain-latency", "cmd-chain-throughput", "df-bw", "tct-one-col", "tct-all-col", "gemm", "aie-reconfig-overhead", "spatial-sharing-overhead", "temporal-sharing-overhead"]
    }]
  }]
}]
)";

// Program entry
int main( int argc, char** argv )
{
  // -- Build the supported subcommands
  SubCmdsCollection subCommands;
  const std::string executable = "xrt-smi";

  boost::property_tree::ptree configTree;
  std::istringstream command_config_stream(command_config);
  boost::property_tree::read_json(command_config_stream, configTree);

  {
    // Syntax: SubCmdClass( IsHidden, IsDepricated, IsPreliminary)
    subCommands.emplace_back(std::make_shared<  SubCmdExamine  >(false, false, false, configTree));
    subCommands.emplace_back(std::make_shared<  SubCmdProgram  >(false, false, false));
    subCommands.emplace_back(std::make_shared<    SubCmdReset  >(false, false, false));
    subCommands.emplace_back(std::make_shared< SubCmdConfigure >(false, false, false, configTree));

    // Parse sub commands from json files
    populateSubCommandsFromJSON(subCommands, executable);

#ifdef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
    subCommands.emplace_back(std::make_shared< SubCmdValidate >(false,  false, false, configTree));
#endif

    subCommands.emplace_back(std::make_shared< SubCmdAdvanced >(true, false, true, configTree));
  }

  for (auto & subCommand : subCommands) {
    subCommand->setExecutableName(executable);
  }

  // -- Program Description
  const std::string description = 
  "The XRT - System Management Interface (xrt-smi) is a standalone"
  " command-line utility that is included with the XRT runtime"
  " package. It includes multiple commands to configure, examine, and validate"
  " supported device(s).\n\nThe reports produced by xrt-smi may be used for device" 
  " administration, monitoring, and troubleshooting application behavior.";

  // -- Ready to execute the code
  try {
    main_( argc, argv, executable, description, subCommands, configTree);
    return 0;
  } catch (const xrt_core::error& e) {
    // Clean exception exit
    // If the exception is "operation_canceled" then don't print the header debug info
    if (e.code().value() != static_cast<int>(std::errc::operation_canceled))
      xrt_core::send_exception_message(e.what(), executable.c_str());
    else // Handle the operation canceled case
      XBUtilities::print_exception(e);
  } catch (const std::exception& e) {
    xrt_core::send_exception_message(e.what(), executable.c_str());
  } catch (...) {
    xrt_core::send_exception_message("Unknown error", executable.c_str());
  }

  return 1;
}
