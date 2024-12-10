// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_Performance.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Performance::OO_Performance( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Modes: default, powersaver, balanced, performance, turbo")
    , m_device("")
    , m_action("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_optionsHidden.add_options()
    ("mode", boost::program_options::value<decltype(m_action)>(&m_action)->required(), "Action to perform: default, powersaver, balanced, performance, turbo") 
  ;

  m_positionalOptions.
    add("mode", 1 /* max_count */)
  ;
}

void
OO_Performance::execute(const SubCmdOptions& _options) const
{
  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::options_description all_options("All Options");
    all_options.add(m_optionsDescription);
    all_options.add(m_optionsHidden);
    po::command_line_parser parser(_options);
    XBUtilities::process_arguments(vm, parser, all_options, m_positionalOptions, true);
  } catch(boost::program_options::error&) {
      if(m_help) {
        printHelp();
        throw xrt_core::error(std::errc::operation_canceled);
      }
      // Exit if neither action or device specified
      if(m_action.empty()) {
        std::cerr << boost::format("ERROR: the required argument for option '--pmode' is missing\n");
        printHelp();
        throw xrt_core::error(std::errc::operation_canceled);
      }
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
      xrt_core::device_update<xrt_core::query::performance_mode>(device.get(), xrt_core::query::performance_mode::power_type::powersaver);
    }
    else if (boost::iequals(m_action, "BALANCED")) {
      xrt_core::device_update<xrt_core::query::performance_mode>(device.get(), xrt_core::query::performance_mode::power_type::balanced);
    }
    else if (boost::iequals(m_action, "PERFORMANCE")) {
      xrt_core::device_update<xrt_core::query::performance_mode>(device.get(), xrt_core::query::performance_mode::power_type::performance);
    }
    else if (boost::iequals(m_action, "TURBO")) {
      xrt_core::device_update<xrt_core::query::performance_mode>(device.get(), xrt_core::query::performance_mode::power_type::turbo);
    }
    else {
      throw xrt_core::error(boost::str(boost::format("Invalid pmode value: '%s'\n") % m_action));
    }
    std::cout << boost::format("\nPower mode is set to %s \n") % (boost::algorithm::to_lower_copy(m_action));
  }
  catch(const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
