// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#include "OO_AutoCoreDump.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"
#include "core/common/query_requests.h"

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <any>
#include <iostream>

namespace po = boost::program_options;

OO_AutoCoreDump::OO_AutoCoreDump(const std::string& _longName, bool _isHidden)
  : OptionOptions(_longName, _isHidden, "Enable|disable automatic coredump on error")
  , m_device("")
  , m_enable(false)
  , m_disable(false)
  , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("enable", boost::program_options::bool_switch(&m_enable), "Enable automatic coredump on error")
    ("disable", boost::program_options::bool_switch(&m_disable), "Disable automatic coredump on error")
  ;
}

void
OO_AutoCoreDump::validate_args() const
{
  if (!m_enable && !m_disable && !m_help)
    throw xrt_core::error(std::errc::operation_canceled, "Please specify an action: --enable or --disable");

  if (m_enable && m_disable)
    throw xrt_core::error(std::errc::operation_canceled, "Cannot specify both --enable and --disable");
}

void
OO_AutoCoreDump::execute(const SubCmdOptions& _options) const
{
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

  const uint32_t enable = m_enable ? 1U : 0U;
  const std::string action_name = m_enable ? "enabled" : "disabled";

  try {
    xrt_core::device_update<xrt_core::query::auto_coredump>(device.get(), std::any(enable));
    std::cout << "Automatic coredump on error " << action_name << std::endl;
  } catch (const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
