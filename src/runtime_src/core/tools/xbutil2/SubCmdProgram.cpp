/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdProgram.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "xrt.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"


// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdProgram::SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("program", 
             "Download the acceleration program to a given device")
{
  const std::string longDescription = "Programs the given acceleration image into the device's shell.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdProgram::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: program");
  // -- Retrieve and parse the subcommand options -----------------------------
  std::vector<std::string> device;
  std::string xclbin;
  bool help = false;

  po::options_description commonOptions("Common Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("user,u", boost::program_options::value<std::string>(&xclbin), "The name (and path) of the xclbin to be loaded")
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options");

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options, commonOptions, hiddenOptions);

  // Check to see if help was requested or no command was found
  if (help) {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  XclBin: %s") % xclbin));

  // -- process "device" option -----------------------------------------------
  //enforce device specification
  if(device.empty()) {
    std::cout << "\nERROR: Device not specified.\n";
    std::cout << "\nList of available devices:" << std::endl;
    boost::property_tree::ptree available_devices = XBU::get_available_devices(true);
    for(auto& kd : available_devices) {
      boost::property_tree::ptree& dev = kd.second;
      std::cout << boost::format("  [%s] : %s\n") % dev.get<std::string>("bdf") % dev.get<std::string>("vbnv");
    }
    std::cout << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : device) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  try {
    XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // enforce 1 device specification
  if(deviceCollection.size() > 1) {
    std::cerr << "\nERROR: Programming multiple device is not supported. Please specify a single device using --device option\n\n";
    std::cout << "List of available devices:" << std::endl;
    boost::property_tree::ptree available_devices = XBU::get_available_devices(true);
    for(auto& kd : available_devices) {
      boost::property_tree::ptree& _dev = kd.second;
      std::cout << boost::format("  [%s] : %s\n") % _dev.get<std::string>("bdf") % _dev.get<std::string>("vbnv");
    }
    std::cout << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // -- process "program" option -----------------------------------------------
  if (!xclbin.empty()) {
    std::ifstream stream(xclbin, std::ios::binary);
    if (!stream)
      throw xrt_core::error(boost::str(boost::format("Could not open %s for reading") % xclbin));

    stream.seekg(0,stream.end);
    size_t size = stream.tellg();
    stream.seekg(0,stream.beg);

    std::vector<char> raw(size);
    stream.read(raw.data(),size);

    std::string v(raw.data(),raw.data()+7);
    if (v != "xclbin2")
      throw xrt_core::error(boost::str(boost::format("Bad binary version '%s'") % v));

    for (const auto & dev : deviceCollection) {
      auto hdl = dev->get_device_handle();
      auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));
      if (auto err = xclLoadXclBin(hdl,reinterpret_cast<const axlf*>(raw.data())))
        throw xrt_core::error(err, "Could not program device " + bdf);

      std::cout << "INFO: xbutil program succeeded on " << bdf << std::endl;
    }
    return;
  }

  std::cout << "\nERROR: Missing program operation. No action taken.\n\n";
  printHelp(commonOptions, hiddenOptions);
  throw xrt_core::error(std::errc::operation_canceled);
}
