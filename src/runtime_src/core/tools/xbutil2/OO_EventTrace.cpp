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
    , m_action("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_optionsHidden.add_options()
    ("action", boost::program_options::value<decltype(m_action)>(&m_action), "Action to perform: enable, disable, status");
  ;

  m_positionalOptions.
    add("action", 1 /* max_count */)
  ;
}

void
OO_EventTrace::validate_args() const {
  if(m_action.empty() && !m_help)
    throw xrt_core::error(std::errc::operation_canceled, "Please specify a action 'enable', 'disable' or 'status'");
  std::vector<std::string> vec_action { "enable", "disable", "status" };
  if (!m_action.empty() && std::find(vec_action.begin(), vec_action.end(), m_action) == vec_action.end()) {
    throw xrt_core::error(std::errc::operation_canceled, boost::str(boost::format("\n'%s' is not a valid action for event trace\n") % m_action));
  }
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

  XBUtilities::sudo_or_throw("Event trace configuration requires admin privileges");

  auto action_to_int = [](const std::string& action) -> uint32_t {
    return action == "enable" ? 1 : 0;
  };

  try {
    xrt_core::device_update<xrt_core::query::event_trace>(device.get(), action_to_int(m_action));
  }
  catch(const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
