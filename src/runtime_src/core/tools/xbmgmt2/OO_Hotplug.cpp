// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_Hotplug.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"

// XRT - Include Files
#include "core/common/query_requests.h"
#include "core/common/system.h"

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <fstream>

static void
hotplug_online()
{
  static const std::string  rescan_path = "/sys/bus/pci/rescan";

  if (!boost::filesystem::exists(rescan_path))
    throw xrt_core::error((boost::format("Invalid sysfs file path '%s'.") % rescan_path).str());

  std::ofstream rescan_file(rescan_path);
  if (!rescan_file.is_open())
    throw xrt_core::error((boost::format("Unable to open the sysfs file '%s'.") % rescan_path).str());

  // Writing "1" to /sys/bus/pci/rescan will trigger the hotplug event.
  rescan_file << 1;
  rescan_file.flush();
  if (!rescan_file.good()) {
    rescan_file.close();
    throw std::runtime_error(boost::str(boost::format("Can't write to file %s") % rescan_path));
  }

  rescan_file.close();
}

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Hotplug::OO_Hotplug(const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Perform hotplug for the given device")
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_devices)>(&m_devices), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("action", po::value<decltype(m_action)>(&m_action)->required(), "Action to perform: online or offline")
    ("help", po::bool_switch(&m_help), "Help to use this sub-command")
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
  for (const auto & aString : _options)
    XBUtilities::verbose(" " + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  if (m_help) {
    printHelp();
    return;
  }

  try {
    if (!boost::iequals(m_action, "online") && !boost::iequals(m_action, "offline")) {
      std::cerr << boost::format("ERROR: Invalid action value: '%s'\n") % m_action;
      printHelp();
      throw xrt_core::error(std::errc::operation_canceled);
    }

    const bool is_offline = boost::iequals(m_action, "offline");

    // Device BDF need to spcify for offline (hot removal) case
    if (is_offline) {
      if (m_devices.empty()) {
        std::cerr << boost::format("ERROR: A device needs to be specified for offline.\n");
        throw xrt_core::error(std::errc::operation_canceled);
      }
    }
    else {
      if (!m_devices.empty()) {
        std::cerr << boost::format("ERROR: Please do not specify any device for online.\n");
        throw xrt_core::error(std::errc::operation_canceled);
      }
    }

    // Find device of interest
    std::shared_ptr<xrt_core::device> dev;

    try {
      dev = XBUtilities::get_device(boost::algorithm::to_lower_copy(m_devices), true /*inUserDomain*/);
    } catch (const std::runtime_error& e) {
      // Catch only the exceptions that we have generated earlier
      std::cerr << boost::format("ERROR: %s\n") % e.what();
      throw xrt_core::error(std::errc::operation_canceled);
    }


    XBUtilities::sudo_or_throw("Root privileges required to perform hotplug operation");
    std::cout << "CAUTION: Performing hotplug command. " <<
	  "This command is going to impact both user pf and mgmt pf.\n" <<
	  "Please make sure no application is currently running." << std::endl;

    // Get permission from user.
    if (!XBUtilities::can_proceed(XBUtilities::getForce()))
      throw xrt_core::error(std::errc::operation_canceled);

    if (is_offline) {
      xrt_core::device_query<xrt_core::query::hotplug_offline>(dev);
    }
    else {
      // For online, no specific device is attached with it. 
      // Hence can't implement as a query. Implemented it locally 
      hotplug_online();
    }

    std::cout << boost::format("\nHotplug %s successfully\n") % (is_offline ? "offline" : "online");
  }
  catch (const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  catch (const std::runtime_error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
