/**
 * Copyright (C) 2021-2022 Xilinx, Inc
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
#include "SubCmdDump.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/common/query_requests.h"
#include "flash/flasher.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// ------ L O C A L   F U N C T I O N S ---------------------------------------

static void
flash_dump(const std::shared_ptr<xrt_core::device>& _dev, const std::string output)
{
  // Sample output:
  //   Output file: foo.bin
  //   Flash Size: 0x222 (Mbits)
  //   <Progress Bar>

  Flasher flasher(_dev->get_device_id());
  if(!flasher.isValid()) {
    xrt_core::error(boost::str(boost::format("%d is an invalid index") % _dev->get_device_id()));
    return;
  }
  flasher.readBack(output);
}

static bool
is_supported(const std::shared_ptr<xrt_core::device>& dev)
{
  bool is_mfg = false;
  bool is_recovery = false;

  try {
    is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(dev);
  } catch(const xrt_core::query::exception&) {}

  try {
    is_recovery = xrt_core::device_query<xrt_core::query::is_recovery>(dev);
  } catch(const xrt_core::query::exception&) {}

  if (is_mfg || is_recovery) {
    std::cerr << boost::format("This operation is not supported with %s image.\n") % (is_mfg ? "manufacturing" : "recovery");
    return false;
  }

  return true;
}

/*
 * so far, we only support the following configs, eg
 * [Device]
 * mailbox_channel_disable = 0x20
 * mailbox_channel_switch = 0
 * xclbin_change = 1
 * cache_xclbin = 0
 */
static void
config_dump(const std::shared_ptr<xrt_core::device>& _dev, const std::string output)
{
  boost::property_tree::ptree ptRoot;

  boost::property_tree::ptree child;
  child.put("mailbox_channel_disable", xrt_core::device_query<xrt_core::query::config_mailbox_channel_disable>(_dev));
  child.put("mailbox_channel_switch", xrt_core::device_query<xrt_core::query::config_mailbox_channel_switch>(_dev));
  child.put("xclbin_change", xrt_core::device_query<xrt_core::query::config_xclbin_change>(_dev));
  child.put("cache_xclbin", xrt_core::device_query<xrt_core::query::cache_xclbin>(_dev));

  if (is_supported(_dev)) {
    try {
      child.put("scaling_enabled", xrt_core::device_query<xrt_core::query::xmc_scaling_enabled>(_dev));
      child.put("scaling_power_override", xrt_core::device_query<xrt_core::query::xmc_scaling_power_override>(_dev));
      child.put("scaling_temp_override", xrt_core::device_query<xrt_core::query::xmc_scaling_temp_override>(_dev));
    } catch(const xrt_core::query::exception&) {}
  }

  ptRoot.put_child("Device", child);

  boost::property_tree::ini_parser::write_ini(output, ptRoot);
  std::cout << "config has been dumped to " << output << std::endl;
}

SubCmdDump::SubCmdDump(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("dump",
             "Dump out the contents of the specified option")
{
  const std::string longDescription = "Dump out the contents of the specified option.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

// ----- C L A S S   M E T H O D S -------------------------------------------

void
SubCmdDump::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: dump");
  // -- Retrieve and parse the subcommand options -----------------------------
  std::vector<std::string> devices;
  std::string output = "";
  bool flash = false;
  bool config = false;
  bool help = false;

  po::options_description commonOptions("Common Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(devices)>(&devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("config,c", boost::program_options::bool_switch(&config), "Dumps the output of system configuration, requires a .ini output file by -o option")
    ("flash,f", boost::program_options::bool_switch(&flash), "Dumps the output of programmed system image, requires a .bin output file by -o option")
    ("output,o", boost::program_options::value<decltype(output)>(&output), "Direct the output to the given file")
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options");

  po::options_description allOptions("All Options");
  allOptions.add(commonOptions);
  allOptions.add(hiddenOptions);

  po::positional_options_description positionals;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(allOptions).positional(positionals).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(commonOptions, hiddenOptions);
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose("SubCmd: Dump");

  // -- process "device" option -----------------------------------------------
  XBU::verbose("Option: device");
  for (auto & str : devices)
    XBU::verbose(std::string(" ") + str);

  if(devices.empty()) {
    std::cerr << "ERROR: Please specify a single device using --device option" << "\n\n";
    printHelp(commonOptions, hiddenOptions);
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;  // The collection of devices to examine
  for (const auto & deviceName : devices)
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  try {
    XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // enforce 1 device specification
  if(deviceCollection.size() != 1) {
    std::cerr << "ERROR: Please specify a single device. Multiple devices are not supported" << "\n\n";
    printHelp(commonOptions, hiddenOptions);
    throw xrt_core::error(std::errc::operation_canceled);
  }

  std::shared_ptr<xrt_core::device>& workingDevice = deviceCollection[0];

  // -- process "output" option -----------------------------------------------
  // Output file
  XBU::verbose("Option: output: " + output);

  if (output.empty()) {
    std::cerr << "ERROR: Please specify an output file using --output option" << "\n\n";
    printHelp(commonOptions, hiddenOptions);
    throw xrt_core::error(std::errc::operation_canceled);
  }
  if (!output.empty() && boost::filesystem::exists(output) && !XBU::getForce()) {
    std::cerr << boost::format("Output file already exists: '%s'") % output << "\n\n";
    throw xrt_core::error(std::errc::operation_canceled);
  }

  //decide the contents of the dump file
  if(flash) {
    flash_dump(workingDevice, output);
    return;
  }
  if (config) {
    config_dump(workingDevice, output);
    return;
  }

  std::cerr << "ERROR: Please specify a valid option to determine the type of dump" << "\n\n";
  printHelp(commonOptions, hiddenOptions);
  throw xrt_core::error(std::errc::operation_canceled);
}
