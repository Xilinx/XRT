// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_Retention.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
namespace po = boost::program_options;

#include <iostream> 

OO_Retention::OO_Retention( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName,
                    "",
                    "Enables / Disables memory retention. Valid values are: [ENABLE | DISABLE]",
                    boost::program_options::value<decltype(m_retention)>(&m_retention),
                    "Enables / Disables memory retention. Valid values are: [ENABLE | DISABLE]",
                    _isHidden)
    , m_device("")
    , m_retention("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

}

/*
 * helper function for option:en(dis)able_retention
 * set data retention
 */
static void 
memory_retention(xrt_core::device* device, bool enable)
{
  XBU::sudo_or_throw("Updating memory retention requires sudo");
  try {
    auto value = xrt_core::query::data_retention::value_type(enable);
    xrt_core::device_update<xrt_core::query::data_retention>(device, value);
  } catch (const std::exception&) {
    std::cerr << boost::format("ERROR: Device does not support memory retention\n\n");
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

void
OO_Retention::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Retention");

  XBUtilities::verbose("Option(s):");
  for (auto & aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  if (m_help) {
    printHelp();
    return;
  }

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), false /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // If in factory mode the device is not ready for use
  if (xrt_core::device_query<xrt_core::query::is_mfg>(device.get())) {
    std::cout << boost::format("ERROR: Device is in factory mode and cannot be configured\n");
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Keeps track if any parameter was updated to prevent no option printout/error
  bool retention_updated = false;

  if (!m_retention.empty()) {
    // Validate the given retention string
    const auto upper_retention = boost::algorithm::to_upper_copy(m_retention);
    bool enableRetention = boost::iequals(upper_retention, "ENABLE");
    bool disableRetention = boost::iequals(upper_retention, "DISABLE");
    if (!enableRetention && !disableRetention) {
      std::cerr << "ERROR: Invalidate '--retention' option: " << m_retention << std::endl;
      printHelp();
      throw xrt_core::error(std::errc::operation_canceled);
    }

    memory_retention(device.get(), enableRetention);
    // Memory retention was updated!
    retention_updated = true;
  }

  if (!retention_updated) {
    std::cerr << "ERROR: Could not update retention" << "\n\n";
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
