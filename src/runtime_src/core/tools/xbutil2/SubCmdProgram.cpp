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
#include "SubCmdProgram.h"
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
    ("device,d", boost::program_options::value<decltype(device)>(&device)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest. A value of 'all' indicates that every found device should be programmed.")
    ("program,p", boost::program_options::value<std::string>(&xclbin), "The name (and path) of the xclbin to be loaded")
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
    return;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
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
    return;
  }

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : device) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);

  //allow programming of only 1 device
  if(deviceCollection.size() > 1)
    throw xrt_core::error("Multiple devices found, please specify one device");

  // -- process "program" option -----------------------------------------------
  if (!xclbin.empty()) {
    std::ifstream stream(xclbin, std::ios::binary);
    if (!stream)
      throw xrt_core::error(boost::str(boost::format("Could not open %s for reding") % xclbin));

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
        throw xrt_core::error(err, "Could not program device" + bdf);

      std::cout << "INFO: xbutil program succeeded on " << bdf << std::endl;
    }
    return;
  }
  std::cout << "\nERROR: Missing program operation. No action taken.\n\n";
  printHelp(commonOptions, hiddenOptions);
}
