// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_ChangeBoot.h"

// XRT - Include Files
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <fstream>
#include <iostream>
// =============================================================================

// ----- H E L P E R M E T H O D S ------------------------------------------


// ----- C L A S S   M E T H O D S -------------------------------------------

OO_ChangeBoot::OO_ChangeBoot(const std::string &_longName, const std::string &_shortName, bool _isHidden )
    : OptionOptions(_longName,
                    _shortName,
                    "Modify the boot for an RPU and/or APU to either partition A or partition B",
                    boost::program_options::value<decltype(boot)>(&boot)->implicit_value("default")->required(),
                    "RPU and/or APU will be booted to either partition A or partition B.  Valid values:\n"
                    "  DEFAULT - Reboot RPU to partition A\n"
                    "  BACKUP  - Reboot RPU to partition B\n",
                    _isHidden),
      m_device(""),
      boot("")
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", po::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

static void
switch_partition(xrt_core::device* device, int boot)
{
  auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  std::cout << boost::format("Rebooting device: [%s] with '%s' partition")
                % bdf % (boot ? "backup" : "default") << std::endl;
  try {
    auto value = xrt_core::query::flush_default_only::value_type(boot);
    xrt_core::device_update<xrt_core::query::boot_partition>(device, value);
    std::cout << "Performing hot reset..." << std::endl;
    auto hot_reset = XBU::str_to_reset_obj("hot");
    device->reset(hot_reset);
    std::cout << "Rebooted successfully" << std::endl;
  }
  catch (const xrt_core::query::exception& ex) {
    std::cout << "ERROR: " << ex.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
    // only available for versal devices
  }
}

void
OO_ChangeBoot::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Change boot");

  XBUtilities::verbose("Option(s):");
  for (const auto & aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), false /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    XBU::throw_cancel(e.what());
  }

  if (boost::iequals(boot, "DEFAULT"))
    switch_partition(device.get(), 0);
  else if (boost::iequals(boot, "BACKUP"))
    switch_partition(device.get(), 1);
  else 
    XBU::throw_cancel(boost::format("Invalid value for boot: %s") % boot);
}
