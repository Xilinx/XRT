/**
 * Copyright (C) 2021-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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

// Local - Include Files
#include <xrt/xrt_device.h>
#include "xrt.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBHelpMenus.h"
#include "tools/common/ReportThermal.h"
#include "tools/common/ReportMechanical.h"
#include "tools/common/ReportElectrical.h"
#include "tools/common/ReportMemory.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>

namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <sstream>
#include <string>

int 
_main(int argc, char* argv[])
{
  std::vector<std::string> devices;

  // Build Options
  po::options_description all_options("All Options");
  all_options.add_options()
    ("device,d", boost::program_options::value<decltype(devices)>(&devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
  ;

    po::positional_options_description positional_command;

  // Parse the command line arguments
  po::variables_map vm;
  po::command_line_parser parser(argc, argv);
  XBU::process_arguments(vm, parser, all_options, positional_command, true);

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : devices) 
      deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  try {
      XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
      // Catch only the exceptions that we have generated earlier
      std::cerr << boost::format("ERROR: %s\n") % e.what();
      throw xrt_core::error(std::errc::operation_canceled);
  }

  if(deviceCollection.size() != 1) {
      std::cerr << "ERROR: Please specify a single device. Multiple devices are not supported" << "\n\n";
      throw xrt_core::error(std::errc::operation_canceled);
  }

  // Initial state is OK (value of 0)
  // If any issues occur below the value will be set to 1 or 2
  int status = 0;
  static const ReportCollection reportsToProcess = {
    std::make_shared<ReportMechanical>(),
    std::make_shared<ReportThermal>(),
    std::make_shared<ReportMemory>(),
    std::make_shared<ReportElectrical>()
  };
  std::vector<std::string> elementsFilter;

  std::stringstream output;
  std::ostringstream oSchemaOutput;
  try {
    XBU::produce_nagios_reports(deviceCollection, reportsToProcess, Report::getSchemaDescription("JSON").schemaVersion, elementsFilter, output, oSchemaOutput);
  } catch (...) {
    status = 2;
  }

  // Output status before all other data
  switch (status) {
    case 0:
      std::cout << "STATUS: OK |";
      break;
    case 1:
      std::cout << "STATUS: WARNING |";
      break;
    default:
      std::cout << "STATUS: FAILURE |";
      break;
  }
  std::cout << output.str() << std::endl;
  return status;
}

int main(int argc, char* argv[])
{
  try {
    xclProbe(); // Call this to load the xrt_core library dynamically. Do not remove me!
    return _main(argc, argv);
  } catch (const std::exception& ex) {
    std::cout << "STATUS: FAILURE\n";
    std::cout << "  Error: " << ex.what() << std::endl;
    return 2;
  }
}