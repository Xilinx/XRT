// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdProgram.h"

#include "OO_ChangeBoot.h"
#include "OO_FactoryReset.h"
#include "OO_UpdateBase.h"
#include "OO_UpdateShell.h"
#include "OO_UpdateXclbin.h"

#include "core/common/error.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>

#ifdef _WIN32
#pragma warning(disable : 4996) //std::asctime
#endif

SubCmdProgram::SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary, const boost::property_tree::ptree& configurations)
    : SubCmd("program",
             "Update image(s) for a given device")
    , m_device("")
    , m_help(false)

{
  const std::string longDescription = "Updates the image(s) for a given device.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_commandConfig = configurations;

  addSubOption(std::make_shared<OO_UpdateBase>("base", "b"));
  addSubOption(std::make_shared<OO_UpdateShell>("shell", "s"));
  addSubOption(std::make_shared<OO_FactoryReset>("revert-to-golden"));
  addSubOption(std::make_shared<OO_UpdateXclbin>("user", "u"));
  addSubOption(std::make_shared<OO_ChangeBoot>("boot", "", true));
}

void
SubCmdProgram::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand: program");

  XBUtilities::verbose("Option(s):");
  for (auto & aString : _options) {
    std::string msg = "   ";
    msg += aString;
    XBUtilities::verbose(msg);
  }

  // Parse sub-command ...
  boost::program_options::variables_map vm;
  auto topOptions = process_arguments(vm, _options, false);

  // Check for a suboption
  auto optionOption = checkForSubOption(vm);

  if (optionOption) {
    optionOption->execute(_options);
    return;
  }

  // Check to see if help was requested or no command was found
  if (m_help) {
    printHelp(false, "", XBUtilities::get_device_class(m_device, true));
    return;
  }

  std::cout << "\nERROR: Missing flash operation.  No action taken.\n\n";
  printHelp(false, "", XBUtilities::get_device_class(m_device, true));
  throw xrt_core::error(std::errc::operation_canceled);
}
