// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_Performance.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Performance::OO_Performance( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Change performance mode of the device")
    , m_device("")
    , m_action("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("action", boost::program_options::value<decltype(m_action)>(&m_action)->required(), "Action to perform: DEFAULT, POWERSAVER, BALANCED, PERFORMANCE")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("action", 1 /* max_count */)
  ;
}

void
OO_Performance::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Performance");

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
  process_arguments(vm, _options);

  if(m_help) {
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  // Exit if neither action or device specified
  if(m_action.empty()) {
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  if(m_device.empty()) {
    std::cerr << boost::format("ERROR: A device needs to be specified.\n");
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  
  try {
    device = XBUtilities::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  try {
    if (boost::iequals(m_action, "DEFAULT")) {
      xrt_core::device_update<xrt_core::query::performance_mode>(device.get(), xrt_core::query::performance_mode::power_type::basic); // default
    }
    else if (boost::iequals(m_action, "POWERSAVER")) {
      xrt_core::device_update<xrt_core::query::performance_mode>(device.get(), xrt_core::query::performance_mode::power_type::low);
    }
    else if (boost::iequals(m_action, "BALANCED")) {
      xrt_core::device_update<xrt_core::query::performance_mode>(device.get(), xrt_core::query::performance_mode::power_type::medium);
    }
    else if (boost::iequals(m_action, "PERFORMANCE")) {
      xrt_core::device_update<xrt_core::query::performance_mode>(device.get(), xrt_core::query::performance_mode::power_type::high);
    }
    else {
      std::cerr << boost::format("ERROR: Invalid action value: '%s'\n") % m_action;
      printHelp();
      throw xrt_core::error(std::errc::operation_canceled);
    }
    std::cout << boost::format("\nPerformance mode is set to %s \n") % (boost::algorithm::to_lower_copy(m_action));
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
