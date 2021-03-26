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
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------


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

  //TO_DO: parse the INI file
}