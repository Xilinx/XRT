/**
 * Copyright (C) 2021 Xilinx, Inc
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
#include "OO_LoadConfig.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

static void load_config(const std::shared_ptr<xrt_core::device>& _dev, const std::string path);

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_LoadConfig::OO_LoadConfig( const std::string &_longName, bool _isHidden)
    : OptionOptions(_longName, _isHidden, "Utility to modify the memory configuration(s)")
    , m_devices({})
    , m_help(false)
    , m_path("")

{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_devices)>(&m_devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("input", boost::program_options::value<decltype(m_path)>(&m_path),"INI file with the memory configuration")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("input", 1 /* max_count */)
  ;
}

void
OO_LoadConfig::execute(const SubCmdOptions& _options) const
{

  XBU::verbose("SubCommand option: load-config");

  XBU::verbose("Option(s):");
  for (auto & aString : _options)
    XBU::verbose(std::string(" ") + aString);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).positional(m_positionalOptions).run(), vm);
    po::notify(vm); // Can throw
  }
  catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    printHelp();
    return;
  }

  // Check the options
  // -- process "help" option -----------------------------------------------
  if (m_help) {
    printHelp();
    return;
  }

  // -- process "device" option -----------------------------------------------
  if(m_devices.empty()) {
    std::cerr << "ERROR: Please specify a single device using --device option" << "\n\n";
    return;
  }

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : m_devices) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  try {
    XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    return;
  }

  // enforce 1 device specification
  if(deviceCollection.size() != 1) {
    std::cerr << "ERROR: Please specify a single device. Multiple devices are not supported" << "\n\n";
    printHelp();
    return;
  }

  std::shared_ptr<xrt_core::device>& workingDevice = deviceCollection[0];

  // -- process "input" option -----------------------------------------------
  if (m_path.empty()) {
    std::cerr << "ERROR: Please specify an input file" << "\n\n";
    printHelp();
    return;
  }
  if (!boost::filesystem::exists(m_path)) {
    std::cerr << boost::format("ERROR: Input file does not exist: '%s'") % m_path << "\n\n";
    return;
  }
  if(boost::filesystem::extension(m_path).compare(".ini") != 0) {
    std::cerr << boost::format("ERROR: Input file should be an INI file: '%s'") % m_path << "\n\n";
    return;
  }

  try {
    load_config(workingDevice, m_path);
    std::cout << "config has been successfully loaded" << std::endl;
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cout << boost::format("ERROR: %s\n") % e.what();
    return;
  }
}

/*
 * so far, we only support the following configs, eg.
 * [Device]
 * mailbox_channel_disable = 0x120
 * mailbox_channel_switch = 0
 * cahce_xclbin = 0
 * we may support in the future, like,
 * [Daemon]
 * host_ip = x.x.x.x
 */
static void load_config(const std::shared_ptr<xrt_core::device>& _dev, const std::string path)
{
  boost::property_tree::ptree ptRoot;
  boost::property_tree::ini_parser::read_ini(path, ptRoot);
  static boost::property_tree::ptree emptyTree;

  const boost::property_tree::ptree PtDevice =
    ptRoot.get_child("Device", emptyTree);

  if (PtDevice.empty())
    throw std::runtime_error("No [Device] section in the config file");

  for (auto& key : PtDevice) {
    if (!key.first.compare("mailbox_channel_disable")) {
      xrt_core::device_update<xrt_core::query::config_mailbox_channel_disable>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    if (!key.first.compare("mailbox_channel_switch")) {
      xrt_core::device_update<xrt_core::query::config_mailbox_channel_switch>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    if (!key.first.compare("cache_xclbin")) {
      xrt_core::device_update<xrt_core::query::cache_xclbin>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    throw std::runtime_error(boost::str(boost::format("'%s' is not a supported config entry") % key.first));
  }
}
