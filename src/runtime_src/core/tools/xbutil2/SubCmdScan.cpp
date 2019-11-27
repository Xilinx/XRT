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
#include "SubCmdScan.h"
#include "XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include "common/device_core.h"

// ======= R E G I S T E R   T H E   S U B C O M M A N D ======================
#include "SubCmd.h"
static const unsigned int registerResult =
                    register_subcommand("scan",
                                        "<add description>",
                                        subCmdScan);
// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------




// ------ F U N C T I O N S ---------------------------------------------------

int subCmdScan(const std::vector<std::string> &_options)
// Reference Command:  scan

{
  XBU::verbose("SubCommand: scan");
  // -- Retrieve and parse the subcommand options -----------------------------
  bool help = false;
  uint64_t card = 0;

  po::options_description scanDesc("scan options");

  scanDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<uint64_t>(&card), "Card to be examined")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(scanDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    xrt_core::send_exception_message(e.what(), "XBUTIL");
    std::cerr << scanDesc << std::endl;

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    std::cout << scanDesc << std::endl;
    return 0;
  }

  auto& core = xrt_core::device_core::instance();

  // Collect
  namespace bpt = boost::property_tree;
  bpt::ptree pt;
  core.get_devices(pt);

  // Walk the property tree and print info
  auto devices = pt.get_child_optional("devices");
  if (!devices || (*devices).size()==0)
    throw xrt_core::error("No devices found");

  for (auto& device : *devices) {
    std::cout << "[" << device.second.get<std::string>("device_id") << "] <board TBD> ...\n";
    // populate with  same output as old xbutil
  }

  return registerResult;
}
