// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_FirmwareLog.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"
#include "core/common/query_requests.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_FirmwareLog::OO_FirmwareLog( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Enable|disable firmware log")
    , m_device("")
    , m_enable(false)
    , m_disable(false)
    , m_help(false)
    , m_log_level(0)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("enable", boost::program_options::bool_switch(&m_enable), "Enable firmware log")
    ("disable", boost::program_options::bool_switch(&m_disable), "Disable firmware log")
    ("log-level", boost::program_options::value<decltype(m_log_level)>(&m_log_level), "Log level (for enable action)")
  ;
  ;
}

void
OO_FirmwareLog::validate_args() const {
  if(!m_enable && !m_disable && !m_help)
    throw xrt_core::error(std::errc::operation_canceled, "Please specify an action: --enable or --disable");
  
  if(m_enable && m_disable)
    throw xrt_core::error(std::errc::operation_canceled, "Cannot specify both --enable and --disable");
}



void
OO_FirmwareLog::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Firmware Log");

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

  if (m_help)
  {
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
    XBUtilities::sudo_or_throw("Firmware log configuration requires admin privileges");

    uint32_t action_value = m_enable ? 1 : 0;
    std::string action_name = m_enable ? "enable" : "disable";

    try {
      xrt_core::query::firmware_log_state::value_type params{action_value, m_log_level};
      xrt_core::device_update<xrt_core::query::firmware_log_state>(device.get(), params);
      std::cout << "Firmware log " << action_name << "d successfully\n";
    }
    catch(const xrt_core::error& e) {
      std::cerr << boost::format("\nERROR: %s\n") % e.what();
      printHelp();
      throw xrt_core::error(std::errc::operation_canceled);
    }
    return;
  }
}
