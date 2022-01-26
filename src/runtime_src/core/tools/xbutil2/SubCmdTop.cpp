/**
 * Copyright (C) 2019-2021 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "xrt.h"
#include "SubCmdTop.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/Process.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdTop::SubCmdTop(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("top", 
             "Live reports monitoring")
{
  const std::string longDescription = "Live reports monitoring.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdTop::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: top");
  // -- Retrieve and parse the subcommand options -----------------------------
  std::vector<std::string> devices;
  std::string refresh_rate = "1";
  bool help = false;

  po::options_description commonOptions("Common Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(devices)>(&devices), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("refresh_rate,r", boost::program_options::value<decltype(refresh_rate)>(&refresh_rate), "Refresh rate in seconds")
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options");

  po::options_description allOptions("All Options");
  allOptions.add(commonOptions);
  allOptions.add(hiddenOptions);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(allOptions).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(commonOptions, hiddenOptions);
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }
  
  std::vector<std::string> args = {"/opt/xilinx/xrt/python/xbtop/xbtop.py", "-d", devices[0], "-r", refresh_rate};

  auto exit_code = XBU::runPythonScript(args);
  if (exit_code != 0)
    throw xrt_core::error(std::errc::operation_canceled);
}

