// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_FirmwareLogExamine.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "core/common/query_requests.h"
#include "tools/common/SmiWatchMode.h"
#include "core/common/json/nlohmann/json.hpp"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <sstream>
#include <memory>
#include <optional>
#include <stdexcept>

namespace po = boost::program_options;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_FirmwareLogExamine::OO_FirmwareLogExamine( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Status|watch firmware log data")
    , m_device("")
    , m_help(false)
    , m_watch(false)
    , m_status(false)
    , m_raw(false)
    , m_watch_mode_offset(0)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("status", boost::program_options::bool_switch(&m_status), "Show firmware log status")
    ("watch", boost::program_options::bool_switch(&m_watch), "Watch firmware log data continuously")
    ("raw", boost::program_options::bool_switch(&m_raw), "Output raw firmware log data (no parsing)")
  ;

  m_positionalOptions.
    add("status", 1 /* max_count */).
    add("watch", 1 /* max_count */)
  ;
}

void
OO_FirmwareLogExamine::validate_args() const {
  // Default behavior is to dump firmware log data once
  // Only explicit actions are --status and --watch
  if(m_status && m_watch)
    throw xrt_core::error(std::errc::operation_canceled, "Cannot specify both --status and --watch");
}

std::string
OO_FirmwareLogExamine::generate_parsed_logs(const xrt_core::device* dev,
                                           const smi::firmware_log_config& config,
                                           bool is_watch) const
{
  std::stringstream ss{};

  // Create and setup buffer for firmware log data
  smi_debug_buffer debug_buf(m_watch_mode_offset, is_watch);

  // Get buffer from driver
  xrt_core::query::firmware_debug_buffer data_buf{};
  try {
    data_buf = xrt_core::device_query<xrt_core::query::firmware_log_data>(dev, debug_buf.get_log_buffer());
  } 
  catch (const std::exception& e) {
    ss << "Error retrieving firmware log data: " << e.what() << "\n";
    m_watch_mode_offset = 0;
    return ss.str();
  }
  
  m_watch_mode_offset = data_buf.abs_offset;

  if (!data_buf.data) {
    ss << "No firmware log data available\n";
    return ss.str();
  }

  // Create parser instance and parse the firmware log buffer directly to string
  smi::firmware_log_parser parser(config);
  auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
  size_t buf_size = data_buf.size;

  ss << parser.parse(data_ptr, buf_size);
  return ss.str();
}

std::string
OO_FirmwareLogExamine::generate_raw_logs(const xrt_core::device* dev,
                                        bool is_watch) const
{
  std::stringstream ss{};

  try {
    // Create and setup buffer for firmware log data
    smi_debug_buffer debug_buf(m_watch_mode_offset, is_watch);

    // Get raw buffer from device for raw dump
    auto data_buf = xrt_core::device_query<xrt_core::query::firmware_log_data>(dev, debug_buf.get_log_buffer());
    
    m_watch_mode_offset = data_buf.abs_offset;
    
    if (!data_buf.data) {
      ss << "No firmware log data available\n";
      return ss.str();
    }

    auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
    size_t buf_size = data_buf.size;

    // Simply print the raw payload data
    ss.write(reinterpret_cast<const char*>(data_ptr), static_cast<std::streamsize>(buf_size));
  } catch (const std::exception& e) {
    ss << "Error retrieving raw firmware log data: " << e.what() << "\n";
  }

  return ss.str();
}

void
OO_FirmwareLogExamine::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Firmware Log Examine");

  XBUtilities::verbose("Option(s):");
  for (auto & aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::options_description all_options("All Options");
    all_options.add(m_optionsDescription);
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

  // Handle status action first
  if (m_status) {
    try {
      auto status = xrt_core::device_query<xrt_core::query::firmware_log_state>(device.get());
      std::cout << "Firmware log status: " << (status.action == 1 ? "enabled" : "disabled") << "\n";
      std::cout << "Firmware log level: " << status.log_level << "\n";
    } catch (const std::exception& e) {
      std::cerr << "Error getting firmware log status: " << e.what() << "\n";
      throw xrt_core::error(std::errc::operation_canceled);
    }
    return;
  }

  // Handle watch or default dump action
  bool is_watch = m_watch;
  
  // Try to parse config unless user explicitly wants raw logs
  std::optional<smi::firmware_log_config> config;

  if (!m_raw) {
    try {
      auto archive = XBUtilities::open_archive(device.get());
      if (!archive) {
        throw std::runtime_error("Failed to open archive");
      }
      auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive.get(), {"firmware_log.json"});
      
      auto& config_data = artifacts_repo["firmware_log.json"];
      std::string config_content(config_data.begin(), config_data.end());
      
      auto json_config = nlohmann::json::parse(config_content);
      config = smi::firmware_log_config(json_config);
    } 
    catch (const std::exception& e) {
      std::cout << "Warning: Dumping raw firmware log: " << e.what() << "\n";
    }
  }

  if (is_watch) {
    // Watch mode
    auto report_generator = [&](const xrt_core::device* dev) -> std::string {
      return (m_raw || !config) 
        ? generate_raw_logs(dev, true)
        : generate_parsed_logs(dev, *config, true);
    };
    smi_watch_mode::run_watch_mode(device.get(), std::cout, report_generator);
  } else {
    // Dump mode
    if (m_raw || !config) {
      std::cout << generate_raw_logs(device.get(), false);
    } else {
      std::cout << "Firmware Log Report\n";
      std::cout << "===================\n\n";
      std::cout << generate_parsed_logs(device.get(), *config, false);
    }
  }
}