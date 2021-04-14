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
#include "xrt.h"
#include "SubCmdReset.h"
#include "core/common/query_requests.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------
static void
pretty_print_action_list(xrt_core::device_collection& deviceCollection, xrt_core::query::reset_type reset)
{
  std::cout << "Performing '" << reset.get_name() << "' on " << std::endl;
  for(const auto & device: deviceCollection) {
      std::cout << boost::format("  -[%s]\n") % 
        xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  }

  if (!reset.get_warning().empty())
    std::cout << "WARNING: " << reset.get_warning() << std::endl;
}

static void
reset_device(xrt_core::device* dev, xrt_core::query::reset_type reset)
{
  if (xrt_core::device_query<xrt_core::query::rom_vbnv>(dev).find("_u30_") != std::string::npos) {
    // u30 reset relies on working SC and SN info. SN is read and saved
    // when FPGA is ready. so even if there is firewall trip now, we expect
    // to be able to get S/N again
    // Having SN info available also implies there is a working SC

    std::string sn;
    sn = xrt_core::device_query<xrt_core::query::xmc_serial_num>(dev);
    if (sn.empty()) {
      throw xrt_core::error(-EINVAL,"Reset relies on S/N, but S/N can't be read from SC");
    }
    std::cout << "Card level reset. This will reset all FPGAs on the card." << std::endl;
  }
  //xocl reset is done through ioctl 
  dev->user_reset(XCL_USER_RESET);
  
  std::cout << boost::format("Successfully reset Device[%s]\n") 
    % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));
}

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
supported(std::string resetType) {
  std::vector<std::string> vec { "user" };
  std::vector<std::string>::iterator it;
  it = std::find (vec.begin(), vec.end(), resetType); 
  if (it == vec.end()) {
    throw xrt_core::error(-ENODEV, "reset not supported");
  }
}

void
SubCmdReset::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: reset");
  // -- Retrieve and parse the subcommand options -----------------------------
  std::vector<std::string> devices = {"all"};
  std::string resetType = "user";
  bool help = false;

  po::options_description commonOptions("Common Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(devices)>(&devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.  A value of 'all' (default) indicates that every found device should be examined.")
    ("type,r", boost::program_options::value<decltype(resetType)>(&resetType)->notifier(supported), "The type of reset to perform. Types resets available:\n"
                                                                       "  user         - Hot reset (default)\n"
                                                                       /*"  aie          - Reset Aie array\n"*/
                                                                       /*"  kernel       - Kernel communication links\n"*/
                                                                       /*"  scheduler    - Scheduler\n"*/
                                                                       /*"  clear-fabric - Clears the accleration fabric with the\n"*/
                                                                       /*"                 shells verify.xclbin image.\n"*/
                                                                       /*"  memory       - Clears the memory block."*/)
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
  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;  // The collection of devices to examine
  for (const auto & deviceName : devices) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  try {
    XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    return;
  }

  xrt_core::query::reset_type type = XBU::str_to_reset_obj(resetType);
  pretty_print_action_list(deviceCollection, type);

  // Ask user for permission
  if(!XBU::can_proceed())
    return;

  //perform reset actions
  for (const auto & dev : deviceCollection) {
    try {
      reset_device(dev.get(), type);
    } catch(const xrt_core::error& e) {
      std::cerr << "ERROR: " << e.what() << std::endl;
      std::cout << boost::format("Reset failed on Device[%s]\n") 
        % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));
    } catch (std::exception& ex) {
      std::cerr << "ERROR:" << ex.what() << std::endl;
      std::cout << boost::format("Reset failed on Device[%s]\n") 
        % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));
    }
  }

  return;
}

