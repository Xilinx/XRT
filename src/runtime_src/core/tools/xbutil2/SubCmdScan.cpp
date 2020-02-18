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
#include "SubCmdScan.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"

// System - Include Files
#include <iostream>

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace po = boost::program_options;

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdScan::SubCmdScan(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("scan", 
             "See replacement functionality in command: 'advanced'")
{
  const std::string longDescription = "<add long description>";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdScan::execute(const SubCmdOptions& _options) const
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
    printHelp(scanDesc);

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(scanDesc);
    return;
  }

  // Collect
  namespace bpt = boost::property_tree;
  bpt::ptree pt;
  xrt_core::get_devices(pt);

  // Walk the property tree and print info
  auto devices = pt.get_child_optional("devices");
  if (!devices || (*devices).size()==0)
    throw xrt_core::error("No devices found");

  std::cout << "INFO: Found total " << (*devices).size() << " card(s), " << "TBD" << " are usable.\n";
  for (auto& device : *devices) {
    auto device_id = device.second.get<unsigned int>("device_id", std::numeric_limits<unsigned int>::max());
    auto udev = xrt_core::get_userpf_device(device_id);
    auto vbnv = xrt_core::device_query<xrt_core::query::rom_vbnv>(udev);
    auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(udev);
    std::cout << "[" << device_id << "]: "
              << xrt_core::query::pcie_bdf::to_string(bdf) << " "
              << vbnv << "\n";
#if 0
    dev->read_ready_status(_pt);
    bool ready = _pt.get<bool>("ready", "false");
    if (ready)
      ready_count++;

    std::cout << "[" << device_id << "]: " << _pt.get<std::string>("vbnv", "N/A") << "(ts=" << _pt.get<std::string>("time_since_epoch", "N/A") << ")" << std::endl;
#endif
  }
}
