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
#include "SubCmdReset.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdReset::SubCmdReset(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("reset", 
             "Resets the given device")
{
  const std::string longDescription = "Resets the given device.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdReset::execute(const SubCmdOptions& _options) const
// Reference Command:  reset [-d card]

{
  XBU::verbose("SubCommand: reset");
  // -- Retrieve and parse the subcommand options -----------------------------
  std::string device = "all";
  std::string reset = "all";
  bool help = false;

  po::options_description commonOptions("Common Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.  A value of 'all' (default) indicates that every found device should be examined.")
    ("type,r", boost::program_options::value<decltype(reset)>(&reset), "The type of reset to perform. Types resets available:\n"
                                                                       "  all          - Perform al lknown resets (default)\n"
                                                                       "  kernel       - Kernel communication links\n"
                                                                       "  scheduler    - Scheduler\n"
                                                                       "  clear-fabric - Clears the accleration fabric with the\n"
                                                                       "                 shells verify.xclbin image.\n"
                                                                       "  memory       - Clears the memory block.")
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

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------

  XBU::error("COMMAND BODY NOT IMPLEMENTED.");
  // TODO: Put working code here

  return;
}

