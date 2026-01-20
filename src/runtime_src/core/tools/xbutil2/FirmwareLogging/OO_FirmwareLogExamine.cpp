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
#include "../OutputStreamHelper.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <optional>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace po = boost::program_options;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_FirmwareLogExamine::OO_FirmwareLogExamine( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Status|watch firmware log data")
    , m_device("")
    , m_help(false)
    , m_watch(false)
    , m_status(false)
    , m_raw(std::nullopt)
    , m_version(false)
    , m_watch_mode_offset(0)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("status", boost::program_options::bool_switch(&m_status), "Show firmware log status")
    ("watch", boost::program_options::bool_switch(&m_watch), "Watch firmware log data continuously")
    ("raw", boost::program_options::value<std::string>()->notifier([this](const std::string& value) { m_raw = value; })->implicit_value(""), 
            "Output raw firmware log data (no parsing). Optionally specify output file. Default is to output to console.")
    ("payload-version", boost::program_options::bool_switch(&m_version), "Show firmware log version")
  ;
}

void
OO_FirmwareLogExamine::validate_args() const {
  if(m_status && m_watch)
    throw xrt_core::error(std::errc::operation_canceled, "Cannot specify both --status and --watch");
}

void
OO_FirmwareLogExamine::
handle_version(const xrt_core::device* device) const {
  try {
    // Get firmware log version
    auto version = xrt_core::device_query<xrt_core::query::firmware_log_version>(device);
    
    // Extract version components from 32-bit integer
    // Format: [product][schema][major][minor] - one byte each
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    uint8_t product = (version >> 24) & 0xFF;
    uint8_t schema = (version >> 16) & 0xFF;
    uint8_t major = (version >> 8) & 0xFF;
    uint8_t minor = version & 0xFF;
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    
    std::cout << boost::format("  %-20s : %u\n") % "Product" % static_cast<unsigned>(product);
    std::cout << boost::format("  %-20s : %u\n") % "Schema" % static_cast<unsigned>(schema);
    std::cout << boost::format("  %-20s : %u\n") % "Major" % static_cast<unsigned>(major);
    std::cout << boost::format("  %-20s : %u\n") % "Minor" % static_cast<unsigned>(minor);
  } catch (const std::exception& e) {
    throw xrt_core::error(std::errc::operation_canceled, "Error getting payload version: " + std::string(e.what()));
  }
}

std::string
OO_FirmwareLogExamine::generate_parsed_logs(const xrt_core::device* dev,
                                           const smi::firmware_log_parser& parser,
                                           bool is_watch) const
{
  std::stringstream ss{};

  smi_debug_buffer debug_buf(m_watch_mode_offset, is_watch);
  auto data_buf = xrt_core::device_query<xrt_core::query::firmware_log_data>(dev, debug_buf.get_log_buffer());
  
  m_watch_mode_offset = data_buf.abs_offset;

  if (!data_buf.data || data_buf.size == 0)
    return ss.str();

  // Use the passed parser instance to parse the firmware log buffer directly to string
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
    smi_debug_buffer debug_buf(m_watch_mode_offset, is_watch);

    // Get raw buffer from device/firmware for raw dump
    auto data_buf = xrt_core::device_query<xrt_core::query::firmware_log_data>(dev, debug_buf.get_log_buffer());
    
    m_watch_mode_offset = data_buf.abs_offset;
    
    if (!data_buf.data) 
      return ss.str();

    auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
    size_t buf_size = data_buf.size;

    ss.write(reinterpret_cast<const char*>(data_ptr), static_cast<std::streamsize>(buf_size));
  } catch (const std::exception& e) {
    ss << "Error retrieving raw firmware log data: " << e.what() << "\n";
  }

  return ss.str();
}

void
OO_FirmwareLogExamine::
handle_logging(const xrt_core::device* device) const {
  XBUtilities::OutputStreamHelper output_helper(m_raw);
  std::ostream& out = output_helper.get_stream();
  
  // Try to parse device specific config unless user explicitly wants raw logs
  std::optional<smi::firmware_log_config> config;
  if (!output_helper.is_raw_mode()) {
    try {
      config = smi::firmware_log_config::load_config(device);
    } 
    catch (const std::exception& e) {
      out << "[Warning]: Dumping raw firmware log: " << e.what() << "\n";
    }
  }

  if (m_watch) {
    if (!output_helper.is_raw_mode() && config) {
      smi::firmware_log_parser parser(*config);
      out << parser.get_header_row();
      
      auto report_generator = [this, &parser](const xrt_core::device* dev) -> std::string {
        return generate_parsed_logs(dev, parser, true);
      };
      smi_watch_mode::run_watch_mode(device, out, report_generator);
    } else {
      // Raw mode: no parser needed
      auto report_generator = [this](const xrt_core::device* dev) -> std::string {
        return generate_raw_logs(dev, true);
      };
      smi_watch_mode::run_watch_mode(device, out, report_generator);
    }
  } else {
    if (output_helper.is_raw_mode() || !config) {
      out << generate_raw_logs(device, false);
    } else {
      out << "Firmware Log Report\n";
      out << "===================\n\n";
      
      smi::firmware_log_parser parser(*config);
      out << parser.get_header_row();
      out << generate_parsed_logs(device, parser, false);
    }
  }
  // output_helper destructor automatically flushes and closes the file
}

void
OO_FirmwareLogExamine::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Firmware Log Examine");
  XBUtilities::sudo_or_throw("Firmware logging requires admin privileges");

  XBUtilities::verbose("Option(s):");
  for (auto & aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  po::variables_map vm;

  try {
    po::options_description all_options("All Options");
    all_options.add(m_optionsDescription);
    po::command_line_parser parser(_options);
    XBUtilities::process_arguments(vm, parser, all_options, m_positionalOptions, true);
  } catch(boost::program_options::error& ex) {
    throw xrt_core::error(std::errc::operation_canceled, ex.what());
  } 

  if (m_help)
  {
    printHelp();
    return;
  }

  try {
    validate_args(); 
  } catch(xrt_core::error& err) {
    printHelp();
    throw err;  // Re-throw without printing
  }

  std::shared_ptr<xrt_core::device> device;
  
  try {
    device = XBUtilities::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if (m_status) {
    try {
      auto status = xrt_core::device_query<xrt_core::query::firmware_log_state>(device.get());
      std::cout << "Firmware log status: " << (status.action == 1 ? "enabled" : "disabled") << "\n";
      std::cout << "Firmware log level: " << status.log_level << "\n";
    } catch (const std::exception& e) {
      throw xrt_core::error(std::errc::operation_canceled, 
          "Error getting firmware log status: " + std::string(e.what()));
    }
    return;
  }

  if (m_version) {
    handle_version(device.get());
    return;
  }
  handle_logging(device.get());
}