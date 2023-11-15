// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_Input.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
namespace po = boost::program_options;

#include <filesystem>
#include <iostream> 

OO_Input::OO_Input( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName,
                    "",
                    "Takes an INI file with configuration details (e.g. memory, clock throttling) and loads them onto the device",
                    boost::program_options::value<decltype(m_path)>(&m_path)->required(),
                    "INI file with configuration details (e.g. memory, clock throttling)",
                    _isHidden)
    , m_device("")
    , m_path("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

}

static void load_config(const std::shared_ptr<xrt_core::device>& _dev, const std::string& path)
{
  boost::property_tree::ptree pt_root;
  boost::property_tree::ini_parser::read_ini(path, pt_root);
  static boost::property_tree::ptree empty_tree;

  const boost::property_tree::ptree& pt_device = pt_root.get_child("Device", empty_tree);

  if (pt_device.empty())
    throw std::runtime_error(boost::str(boost::format("No [Device] section in the config file. Config File: %s") % path));

  for (const auto& key : pt_device) {
    if (!key.first.compare("mailbox_channel_disable")) {
      xrt_core::device_update<xrt_core::query::config_mailbox_channel_disable>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    if (!key.first.compare("mailbox_channel_switch")) {
      xrt_core::device_update<xrt_core::query::config_mailbox_channel_switch>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    if (!key.first.compare("xclbin_change")) {
      xrt_core::device_update<xrt_core::query::config_xclbin_change>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    if (!key.first.compare("cache_xclbin")) {
      xrt_core::device_update<xrt_core::query::cache_xclbin>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    bool is_versal = xrt_core::device_query<xrt_core::query::is_versal>(_dev);
    if (is_versal) {
      try {
        if (!key.first.compare("throttling_enabled")) {
          xrt_core::device_update<xrt_core::query::xgq_scaling_enabled>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
        if (!key.first.compare("throttling_power_override")) {
          xrt_core::device_update<xrt_core::query::xgq_scaling_power_override>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
        if (!key.first.compare("throttling_temp_override")) {
          xrt_core::device_update<xrt_core::query::xgq_scaling_temp_override>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
      } catch(const xrt_core::query::exception&) {}
    } else {
      try {
        if (!key.first.compare("throttling_enabled")) {
          xrt_core::device_update<xrt_core::query::xmc_scaling_enabled>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
        if (!key.first.compare("throttling_power_override")) {
          xrt_core::device_update<xrt_core::query::xmc_scaling_power_override>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
        if (!key.first.compare("throttling_temp_override")) {
          xrt_core::device_update<xrt_core::query::xmc_scaling_temp_override>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
      } catch(const xrt_core::query::exception&) {}
    }
    throw std::runtime_error(boost::str(boost::format("'%s' is not a supported config entry") % key.first));
  }
}

void
OO_Input::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Input");

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

  // Load Config commands
  // -- process "input" option -----------------------------------------------
  if (!m_path.empty()) {
    if (!std::filesystem::exists(m_path)) {
      std::cerr << boost::format("ERROR: Input file does not exist: '%s'") % m_path << "\n\n";
      throw xrt_core::error(std::errc::operation_canceled);
    }

    if (std::filesystem::path(m_path).extension().string() == ".ini") {
      std::cerr << boost::format("ERROR: Input file should be an INI file: '%s'") % m_path << "\n\n";
      throw xrt_core::error(std::errc::operation_canceled);
    }

    try {
      load_config(device, m_path);
      std::cout << "Config has been successfully loaded" << std::endl;
      return;
    } catch (const std::runtime_error& e) {
      // Catch only the exceptions that we have generated earlier
      std::cout << boost::format("ERROR: %s\n") % e.what();
      throw xrt_core::error(std::errc::operation_canceled);
    }
  }

  std::cout << "\nERROR: Missing input file. No action taken.\n\n";
  printHelp();
  throw xrt_core::error(std::errc::operation_canceled);
}
