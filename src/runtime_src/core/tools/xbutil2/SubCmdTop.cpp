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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdTop.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdTop::SubCmdTop(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("top", 
             "Display's card activities")
{
  const std::string longDescription = "Display's card's activity";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdTop::execute(const SubCmdOptions& _options) const
// Reference Command:  top  [-i seconds]

{
  XBU::verbose("SubCommand: top");
  // -- Retrieve and parse the subcommand options -----------------------------
  std::string device = "";
  uint64_t seconds = 0;
  bool help = false;

  po::options_description topDesc("Options");
  topDesc.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device)->required(), "The device of interest. This is specified as follows:\n"
                                                                                       "  <BDF> - Bus:Device.Function (e.g., 0000:d8:00.0)\n")
    ("interval,s", boost::program_options::value<uint64_t>(&seconds), "Seconds")
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(topDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(topDesc);

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(topDesc);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("Seconds: %ld") % seconds));

  XBU::error("COMMAND BODY NOT IMPLEMENTED.");
  // TODO: Put working code here
}

