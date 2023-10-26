// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdProgram.h"
#include "OO_Program_Qspips.h"
#include "OO_Program_Spi.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream> 
#include <boost/format.hpp>
#include <map>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdProgram::SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("program", 
             "Updates the image(s) for a given device")
    , m_help(false)
{
  const std::string longDescription = "Programs the given acceleration image into the device's shell.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  addSubOption(std::make_shared<OO_Program_Spi>("spi"));
  addSubOption(std::make_shared<OO_Program_Qspips>("qspips"));
}


void
SubCmdProgram::execute(const SubCmdOptions& _options) const
{
  // 1) Process the common top level options 
  po::variables_map vm;
  auto topOptions = process_arguments(vm, _options, false);

  // Find the subOption;
  auto optionOption = checkForSubOption(vm);

  // No suboption print help
  if (!optionOption) {
      if (m_help) {
          printHelp();
          return;
      }
      std::cerr << "\nERROR: Suboption missing" << std::endl;
      printHelp();
      throw std::errc::operation_canceled;
  }

  // 2) Process the top level options
  if (m_help)
    topOptions.push_back("--help");

  optionOption->setGlobalOptions(getGlobalOptions());
  
  // Execute the option
  optionOption->execute(topOptions);
}
