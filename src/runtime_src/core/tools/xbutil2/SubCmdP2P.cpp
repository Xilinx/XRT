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
#include "SubCmdP2P.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ======= R E G I S T E R   T H E   S U B C O M M A N D ======================
#include "tools/common/SubCmd.h"
static const unsigned int registerResult = 
                    register_subcommand("p2p", 
                                        "<add description>",
                                        subCmdP2P);
// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------




// ------ F U N C T I O N S ---------------------------------------------------

int subCmdP2P(const std::vector<std::string> &_options)
// Reference Command:  
//                      p2p  [-d card] --enable
//                      p2p  [-d card] --disable
//                      p2p  [-d card] --validate

{
  XBU::verbose("SubCommand: p2p");
  // -- Retrieve and parse the subcommand options -----------------------------
  uint64_t card = 0;
  bool bEnable = false;
  bool bDisable = false;
  bool bValidate = false;
  bool help = false;

  po::options_description p2pDesc("p2p options");
  p2pDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<uint64_t>(&card), "Card to be examined")
    ("enable", boost::program_options::bool_switch(&bEnable), "Enable")
    ("disable", boost::program_options::bool_switch(&bDisable), "Disable")
    ("validate", boost::program_options::bool_switch(&bValidate), "Validate")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(p2pDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << p2pDesc << std::endl;

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    std::cout << p2pDesc << std::endl;
    return 0;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(XBU::format("    Card: %ld", card));
  XBU::verbose(XBU::format("  Enable: %d", bEnable));
  XBU::verbose(XBU::format(" Disable: %d", bDisable));
  XBU::verbose(XBU::format("Validate: %d", bValidate));


  XBU::error("COMMAND BODY NOT IMPLEMENTED.");
  // TODO: Put working code here

  return registerResult;
}

