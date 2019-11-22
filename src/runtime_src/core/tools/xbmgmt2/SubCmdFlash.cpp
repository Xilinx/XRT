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
#include "SubCmdFlash.h"
#include "XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ======= R E G I S T E R   T H E   S U B C O M M A N D ======================
#include "SubCmd.h"
static const unsigned int registerResult = 
                    register_subcommand("flash", 
                                        "Update SC firmware or shell on the device",
                                        subCmdFlash);
// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------




// ------ F U N C T I O N S ---------------------------------------------------

int subCmdFlash(const std::vector<std::string> &_options)
// Reference Command:   'flash' sub-command usage:
//                      --scan [--verbose|--json]
//                      --update [--shell name [--id id]] [--card bdf] [--force]
//                      --factory_reset [--card bdf]
//                      
//                      Experts only:
//                      --shell --path file --card bdf [--type flash_type]
//                      --sc_firmware --path file --card bdf


{
  XBU::verbose("SubCommand: flash");
  // -- Retrieve and parse the subcommand options -----------------------------
  bool help = false;
  bool scan = false;
  bool reset = false;
  std::string bdf = "";
  bool shell = false;
  bool sc_firmware = false;
  bool update = false;
  bool force = false;
  std::string file_path = "";
  

  po::options_description flashDesc("flash options");
  flashDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    ("scan", boost::program_options::bool_switch(&scan), "Information about the card")
    ("factory_reset", boost::program_options::bool_switch(&reset), "Reset to golden image")
    ("update", boost::program_options::bool_switch(&update), "Update the card with the installed shell")
    ("force", boost::program_options::bool_switch(&force), "force") //option
    ("card", boost::program_options::value<std::string>(&bdf), "bdf of the card") //option
  ;

  po::options_description expertsOnlyDesc("experts only");
  expertsOnlyDesc.add_options()
    ("shell", boost::program_options::bool_switch(&shell), "Flash platform from source")
    ("sc_firmware", boost::program_options::bool_switch(&sc_firmware), "Flash sc firmware from source")
    ("path", boost::program_options::value<std::string>(&file_path), "path of shell or sc_firmware files") //option
  ;

  po::options_description allOptions("");
  allOptions.add(flashDesc).add(expertsOnlyDesc);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(allOptions).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << allOptions << std::endl;

    // Re-throw exception
    throw;
  }
  // Check to see if help was requested
  if (help == true)  {
    std::cout << allOptions << std::endl;
    return 0;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(XBU::format("  Card: %s", bdf.c_str()));
  XBU::verbose(XBU::format("  Scan: %ld", scan));
  XBU::verbose(XBU::format("  Shell: %ld", shell));
  XBU::verbose(XBU::format("  sc_firmware: %ld", sc_firmware));
  XBU::verbose(XBU::format("  Reset: %ld", reset));
  XBU::verbose(XBU::format("  Update: %ld", update));
  XBU::verbose(XBU::format("  Force: %ld", force));
  XBU::verbose(XBU::format("  File path: %s", file_path.c_str()));



  XBU::error("COMMAND BODY NOT IMPLEMENTED.");
  // TODO: Put working code here
  if (scan) {
      std::cout << "Call xbmgmt flash --scan\n"; //parse verbose|json??
      return registerResult;
  }

  if (update) {
    //   if (shell.empty() && !id.empty()) //--shell name [--id id]
    //     return RC_ERROR_IN_COMMAND_LINE;
      std::cout << "Call xbmgmt update\n"; //call autoFlash
  }

  if (reset) {
      std::cout << "Call xbmgmt reset\n";

  }

  return registerResult;
}

