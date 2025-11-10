// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_EventTrace.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_EventTrace::OO_EventTrace( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Enable|disable event trace")
    , m_device("")
    , m_enable(false)
    , m_disable(false)
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("enable", boost::program_options::bool_switch(&m_enable), "Enable event tracing")
    ("disable", boost::program_options::bool_switch(&m_disable), "Disable event tracing")
    ("categories", boost::program_options::value<decltype(m_categories)>(&m_categories), "Category mask to enable categories")
  ;
}

void
OO_EventTrace::validate_args() const {
  if(!m_enable && !m_disable && !m_help)
    throw xrt_core::error(std::errc::operation_canceled, "Please specify an action: --enable or --disable");
  
  if(m_enable && m_disable)
    throw xrt_core::error(std::errc::operation_canceled, "Cannot specify both --enable and --disable");
}

void
OO_EventTrace::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Event Trace");

  XBUtilities::verbose("Option(s):");
  for (auto & aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::options_description all_options("All Options");
    all_options.add(m_optionsDescription);
    all_options.add(m_optionsHidden);
    po::command_line_parser parser(_options);
    XBUtilities::process_arguments(vm, parser, all_options, m_positionalOptions, true);
  } catch(boost::program_options::error& ex) {
    std::cout << ex.what() << std::endl;
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  } 

  if (m_help) {
    printHelp();
    return;
  }

  try {
    //validate required arguments
    validate_args(); 
  } catch(xrt_core::error& err) {
    std::cout << err.what() << std::endl;
    printHelp();
    throw xrt_core::error(err.get_code());
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

  if (m_enable || m_disable) {
    // Configuration actions require admin privileges
    XBUtilities::sudo_or_throw("Event trace configuration requires admin privileges");

    uint32_t action_value = m_enable ? 1 : 0;
    std::string action_name = m_enable ? "enable" : "disable";

    try {
      xrt_core::query::event_trace_state::value_type params{action_value, m_categories};
      xrt_core::device_update<xrt_core::query::event_trace_state>(device.get(), params);
      std::cout << "Event trace " << action_name << "d successfully\n";
    }
    catch(const xrt_core::error& e) {
      std::cerr << boost::format("\nERROR: %s\n") % e.what();
      printHelp();
      throw xrt_core::error(std::errc::operation_canceled);
    }
    return;
  }
}
