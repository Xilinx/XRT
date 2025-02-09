// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_AieClockFreq.h"
#include "OO_AieRegRead.h"
#include "OO_MemRead.h"
#include "OO_MemWrite.h"
#include "SubCmdAdvanced.h"

#include "common/device.h"
#include "common/system.h"
#include "common/xclbin_parser.h"

#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream> 

#include "common/system.h"
#include "common/device.h"
#include <boost/format.hpp>
#include <map>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdAdvanced::SubCmdAdvanced(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations)
    : SubCmd("advanced", 
             "Low level command operations"),
      m_device("")
{
  const std::string longDescription = "Low level command operations.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_commandConfig = configurations;

  addSubOption(std::make_shared<OO_MemRead>("read-mem"));
  addSubOption(std::make_shared<OO_MemWrite>("write-mem"));
// Only defined for embedded platform
#ifndef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
  addSubOption(std::make_shared<OO_AieRegRead>("read-aie-reg"));
  addSubOption(std::make_shared<OO_AieClockFreq>("aie-clock"));
#endif
}


void
SubCmdAdvanced::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: advanced");

  // =========== Process the options ========================================

  // 1) Process the common top level options 
  po::variables_map vm;
  // Used for the suboption arguments
  auto topOptions = process_arguments(vm, _options, false);

  // Check for a suboption
  auto optionOption = checkForSubOption(vm);

  // No suboption print help
  if (!optionOption || m_help) {
    printHelp(false, "", XBU::get_device_class(m_device, true));
    return;
  }

  optionOption->setGlobalOptions(getGlobalOptions());
  
  // Execute the option
  optionOption->execute(_options);
}
