// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
#include "OO_ChangeBoot.h"

// XRT - Include Files
#include "core/common/query_requests.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <fstream>
#include <iostream>

static void
switch_partition(xrt_core::device* device, int boot)
{
  auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  std::cout << boost::format("Rebooting device: [%s] with '%s' partition")
                % bdf % (boot ? "backup" : "default") << std::endl;
  try {
    // sets sysfs node boot_from_back [1:backup, 0:default], then hot reset
    auto value = xrt_core::query::flush_default_only::value_type(boot);
    xrt_core::device_update<xrt_core::query::boot_partition>(device, value);

    std::cout << "Performing hot reset..." << std::endl;
    device->device_shutdown();
    device->device_online();
    std::cout << "Rebooted successfully" << std::endl;
  }
  catch (const xrt_core::query::exception& ex) {
    std::cout << "ERROR: " << ex.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
    // only available for versal devices
  }
}

OO_ChangeBoot::OO_ChangeBoot(const std::string& _longName, const std::string& _shortName, bool _isHidden)
    : OptionOptions(_longName,
                    _shortName,
                    "Modify the boot for an RPU and/or APU to either partition A or partition B",
                    boost::program_options::value<decltype(m_boot)>(&m_boot)->implicit_value("default")->required(),
                    "RPU and/or APU will be booted to either partition A or partition B.  Valid values:\n"
                    "  DEFAULT - Reboot RPU to partition A\n"
                    "  BACKUP  - Reboot RPU to partition B\n",
                    _isHidden)
    , m_device("")
    , m_boot("")
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", po::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
OO_ChangeBoot::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: program");

  XBU::verbose("Option(s):");
  for (auto & aString : _options) {
    std::string msg = "   ";
    msg += aString;
    XBU::verbose(msg);
  }

  // Parse sub-command ...
  po::variables_map vm;
  auto topOptions = process_arguments(vm, _options);

  // Check to see if help was requested or no command was found
  if (m_help) {
    printHelp();
    return;
  }

  // -- process "device" option -----------------------------------------------
  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), false /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // -- process "boot" option ------------------------------------------
  if (!m_boot.empty()) {
    if (boost::iequals(m_boot, "DEFAULT"))
      switch_partition(device.get(), 0);
    else if (boost::iequals(m_boot, "BACKUP"))
      switch_partition(device.get(), 1);
    else {
      std::cout << "ERROR: Invalid value.\n"
                << "Usage: xbmgmt program --device='0000:00:00.0' --boot [default|backup]"
                << std::endl;
      throw xrt_core::error(std::errc::operation_canceled);
    }
    return;
  }

  std::cout << "\nERROR: Missing flash operation.  No action taken.\n\n";
  printHelp();
  throw xrt_core::error(std::errc::operation_canceled);
}
