// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#include "OO_HardwareContextReset.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"
#include "core/common/query_requests.h"

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <any>
#include <iostream>

namespace po = boost::program_options;

OO_HardwareContextReset::OO_HardwareContextReset(const std::string& _longName, bool _isHidden)
  : OptionOptions(_longName, _isHidden, "Hardware context reset recovery (enable|disable)")
  , m_device("")
  , m_reset_on_error("")
  , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("reset-on-error", boost::program_options::value<decltype(m_reset_on_error)>(&m_reset_on_error), "enable or disable hardware context reset recovery")
  ;
}

void
OO_HardwareContextReset::validate_args() const
{
  if (m_reset_on_error.empty() && !m_help)
    throw xrt_core::error(std::errc::operation_canceled, "Please specify --reset-on-error with enable or disable");

  if (!m_reset_on_error.empty() && !boost::iequals(m_reset_on_error, "enable") && !boost::iequals(m_reset_on_error, "disable"))
    throw xrt_core::error(std::errc::operation_canceled, "Invalid --reset-on-error value (use enable or disable)");
}

void
OO_HardwareContextReset::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: hardware-context reset");
  XBUtilities::sudo_or_throw("Hardware context reset policy requires admin privileges");

  XBUtilities::verbose("Option(s):");
  for (auto& aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  po::variables_map vm;

  try {
    po::options_description all_options("All Options");
    all_options.add(m_optionsDescription);
    all_options.add(m_optionsHidden);
    po::command_line_parser parser(_options);
    XBUtilities::process_arguments(vm, parser, all_options, m_positionalOptions, true);
  } catch (boost::program_options::error& ex) {
    std::cout << ex.what() << std::endl;
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if (m_help) {
    printHelp();
    return;
  }

  try {
    validate_args();
  } catch (xrt_core::error& err) {
    std::cout << err.what() << std::endl;
    printHelp();
    throw xrt_core::error(err.get_code());
  }

  std::shared_ptr<xrt_core::device> device;

  try {
    device = XBUtilities::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  const uint32_t enable = boost::iequals(m_reset_on_error, "enable") ? 1U : 0U;
  const std::string action_name = enable ? "enabled" : "disabled";

  try {
    xrt_core::device_update<xrt_core::query::hardware_context_reset>(device.get(), std::any(enable));
    std::cout << "Hardware context reset recovery " << action_name << std::endl;
  } catch (const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
