/**
 * Copyright (C) 2022 Xilinx, Inc
 * 
 * Licensed under the Apache License, Version
 * 2.0 (the "License"). You may not use this file except in
 * compliance with the License. A copy of the License is located
 * at
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
#include "OO_Hotplug.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"

#include "core/common/system.h"
#include "core/common/query_requests.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
namespace po = boost::program_options;

// =============================================================================

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Hotplug::OO_Hotplug( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Perform hotplug for the given device")
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_devices)>(&m_devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("action", boost::program_options::value<decltype(m_action)>(&m_action)->required(), "Action to perform: online or offline")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("action", 1 /* max_count */)
  ;
}

void
OO_Hotplug::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Hotplug");

  XBUtilities::verbose("Option(s):");
  for (auto & aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).positional(m_positionalOptions).run(), vm);
    po::notify(vm); // Can throw
  }
  catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if(m_help) {
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  // Exit if neither action or device specified
  if(m_action.empty()) {
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  try {
    if (!boost::iequals(m_action, "online") && !boost::iequals(m_action, "offline")) {
      std::cerr << boost::format("ERROR: Invalid action value: '%s'\n") % m_action;
      printHelp();
      throw xrt_core::error(std::errc::operation_canceled);
    }
    bool is_online = boost::iequals(m_action, "online");

    // Device BDF need to spcify for offline (hot removal) case
    if (!is_online) {
      if(m_devices.empty()) {
        std::cerr << boost::format("ERROR: A device needs to be specified.\n");
        throw xrt_core::error(std::errc::operation_canceled);
      }
    }
    else {
      if(!m_devices.empty()) {
        std::cerr << boost::format("ERROR: Please do not specify any device for online.\n");
        throw xrt_core::error(std::errc::operation_canceled);
      }
    }

    // Collect all the devices of interest
    std::set<std::string> deviceNames;
    xrt_core::device_collection deviceCollection;
    for (const auto & deviceName : m_devices) 
      deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));
    
    XBUtilities::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);

    // enforce 1 device specification
    if(deviceCollection.size() > 1) {
      std::stringstream errmsg;
      errmsg << "Multiple devices are not supported. Please specify a single device using --device option\n\n";
      errmsg << "List of available devices:\n";
      boost::property_tree::ptree available_devices = XBUtilities::get_available_devices(true);
      for(auto& kd : available_devices) {
        boost::property_tree::ptree& _dev = kd.second;
        errmsg << boost::format("  [%s] : %s\n") % _dev.get<std::string>("bdf") % _dev.get<std::string>("vbnv");
      }
      throw xrt_core::error(std::errc::operation_canceled, errmsg.str());
    }

    XBUtilities::sudo_or_throw("Root privileges required to perform hotplug operation");
    std::cout << "CAUTION: Performing hotplug command. " <<
	  "This command is going to impact both user pf and mgmt pf.\n" <<
	  "Please make sure no application is currently running." << std::endl;

    // Get permission from user.
    if(!XBUtilities::can_proceed(XBUtilities::getForce()))
      throw xrt_core::error(std::errc::operation_canceled);

    if (is_online) {
      // For Online we don't need any specific device. We are passing first device 
      // just for accessing the sysfs entry i.e. /sys/bus/pci/rescan 
      auto dev = xrt_core::get_mgmtpf_device(0);
      xrt_core::device_query<xrt_core::query::hotplug_online>(dev);
    }
    else {
      auto &dev = deviceCollection[0];
      xrt_core::device_query<xrt_core::query::hotplug_offline>(dev);
    }

    std::cout << boost::format("\nHotplug %s successfully\n") % (is_online ? "online" : "offline");
  }
  catch(const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  catch (const std::runtime_error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
