/**
 * Copyright (C) 2020 Xilinx, Inc
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
#include "SubCmdStatus.h"
#include "XBReport.h"

#include "common/system.h"
#include "common/device.h"
#include "common/xclbin_parser.h"
#include "flash/flasher.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include "core/common/utils.h"
#include "core/common/message.h"

#include "tools/common/XBUtilities.h"

namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>

namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdStatus::SubCmdStatus(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("status", 
             "Returns detail information for the specified device.")
{
  const std::string longDescription = "This command will 'examine' the state of the system/device and will"
                                      " generate a report of interest in a text or JSON format.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdStatus::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: status");

  XBU::verbose("Option(s):");
  for (auto & aString : _options) {
    std::string msg = "   ";
    msg += aString;
    XBU::verbose(msg);
  }

  // -- Retrieve and parse the subcommand options -----------------------------
  std::string device = "";
  std::string report = "";
  std::string format = "";
  std::string output;
  bool help = false;

  po::options_description queryDesc("Options");  // Note: Boost will add the colon.
  queryDesc.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.  A value of 'all' (default) indicates that every found device should be examined.")
    ("report,r", boost::program_options::value<decltype(report)>(&report)->implicit_value("scan"), "The type of report to be produced. Reports currently available are:\n"
                                                                           "  all         - All known reports are produced\n"
                                                                           "  scan        - Terse report of found devices (default)\n"
                                                                           "  electrical  - Voltages, currents, and power\n"
                                                                           "                consumption on the device\n"
                                                                           "  temperature - Temperatures across the device\n"
                                                                           "  os-info     - Information relating to the operating\n"
                                                                           "                system and drivers\n"
                                                                           "  debug-ip    - Debug IP Status\n"
                                                                           "  fans        - Fan status")
    ("format,f", boost::program_options::value<decltype(format)>(&format)->implicit_value("text"), "Report output format. Valid values are:\n"
                                                                           "  text        - Human readable report (default)\n"
                                                                           "  json-2020.1 - JSON 2020.1 schema")
    ("output,o", boost::program_options::value<decltype(output)>(&output), "Direct the output to the given file")
    ("help,h", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(queryDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(queryDesc);
    throw; // Re-throw exception
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(queryDesc);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  // get all device IDs to be processed
  std::vector<uint16_t> device_indices;
  XBU::parse_device_indices(device_indices, device);

  bool output_json;
  // -- Option: format ------------------------------------------------------------------
  // if the user doesn't specify a format or specifies "text", print the human readable format
  if(format.empty() || format.compare("text") == 0) {
    output_json = false;
  } else if (format.compare("json") == 0) {
    output_json = true;
  }

  boost::property_tree::ptree devices_info;
  // -- Option: report ------------------------------------------------------------------
  if(!report.empty()) {
    XBU::verbose("Sub command: --report");

    // Don't know how to deal with "all" yet
    if(report.compare("all") == 0)
      std::cout << "TODO: implement ALL report\n";
    else if(report.compare("temperature") == 0)
      XBReport::report_thermal_devices(device_indices, devices_info, output_json);
    else if(report.compare("electrical") == 0)
      XBReport::report_electrical_devices(device_indices, devices_info, output_json);
    else if(report.compare("os-info") == 0)
      std::cout << "TODO: implement OS-INFO report\n";
    else if(report.compare("debug-ip") == 0)
      std::cout << "TODO: implement DEBUG-IP report\n";
    else if(report.compare("fans") == 0)
      XBReport::report_fans_devices(device_indices, devices_info, output_json);
    else if(report.compare("scan") == 0)
      XBReport::report_shell_on_devices(device_indices, devices_info, output_json);
    else 
      throw xrt_core::error("Please specify a valid value");
  }



}
