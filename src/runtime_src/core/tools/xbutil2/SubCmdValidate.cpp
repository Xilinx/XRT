/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include "SubCmdValidate.h"
#include "XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ------ L O C A L   F U N C T I O N S ---------------------------------------




// ------ F U N C T I O N S ---------------------------------------------------

int subCmdValidate(const std::vector<std::string> &_options, bool _help)
// Reference Command:  clock   [-d card] [-r region] [-f clock1_freq_MHz] [-g clock2_freq_MHz] [-h clock3_freq_MHz]
//                     Change the clock frequency of region 0 in card 0 to 100 MHz\n";
//                         xbutil clock -f 100
//                     For card 0 which supports multiple clocks, change the clock 1 to 200MHz and clock 2 to 250MHz\n";
//                         xbutil clock -f 200 -g 250;

{
  XBU::verbose("SubCommand: validate");
  // -- Retrieve and parse the subcommand options -----------------------------
  uint64_t card = 0;

  po::options_description validateDesc("validate options");
  validateDesc.add_options()
    (",d", boost::program_options::value<uint64_t>(&card), "Card to be examined")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(validateDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << validateDesc << std::endl;

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (_help == true)  {
    std::cout << validateDesc << std::endl;
    return 0;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(XBU::format("  Card: %ld", card));

  XBU::error("COMMAND BODY NOT IMPLEMENTED.");
  // TODO: Put working code here

  return 0;
}

