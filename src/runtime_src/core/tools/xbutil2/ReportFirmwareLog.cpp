// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportFirmwareLog.h"
#include "core/common/query_requests.h"
#include "core/common/time.h"
#include "tools/common/SmiWatchMode.h"
#include "tools/common/XBUtilities.h"
#include "core/common/json/nlohmann/json.hpp"

#include <algorithm>
#include <boost/format.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <fstream>
#include <memory>

using bpt = boost::property_tree::ptree;
namespace XBU = XBUtilities;

std::string
ReportFirmwareLog::generate_parsed_logs(const xrt_core::device* dev,
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
  const auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
  size_t buf_size = data_buf.size;

  ss << parser.parse(data_ptr, buf_size);
  return ss.str();
}

std::string
ReportFirmwareLog::generate_raw_logs(const xrt_core::device* dev,
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

    const auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
    size_t buf_size = data_buf.size;

    // Simply print the raw payload data
    ss.write(reinterpret_cast<const char*>(data_ptr), buf_size);
  } catch (const std::exception& e) {
    ss << "Error retrieving raw firmware log data: " << e.what() << "\n";
  }

  return ss.str();
}

void
ReportFirmwareLog::
getPropertyTreeInternal(const xrt_core::device* dev, bpt& pt) const
{
  getPropertyTree20202(dev, pt);
}

void
ReportFirmwareLog::
getPropertyTree20202(const xrt_core::device* /*dev*/, bpt& /*pt*/) const // Stubbing out for now till we decide on whether to add json dump support
{}

void
ReportFirmwareLog::
writeReport(const xrt_core::device* device,
            const bpt& /*pt*/,
            const std::vector<std::string>& elements_filter,
            std::ostream& output) const
{
  bool user_wants_raw = std::find(elements_filter.begin(), elements_filter.end(), "raw") != elements_filter.end();

  if (std::find(elements_filter.begin(), elements_filter.end(), "status") != elements_filter.end()) {
    auto status = xrt_core::device_query<xrt_core::query::firmware_log_state>(device);
    output << "Firmware log status: "<<(status.action == 1 ? "enabled" : "disabled") <<std::endl;
    output << "Firmware log level: "<< status.log_level << std::endl;
    return;
  }
  
  // Try to parse config unless user explicitly wants raw logs
  std::optional<smi::firmware_log_config> config;

  if (!user_wants_raw) {
    try {
      auto archive = XBU::open_archive(device);
      auto artifacts_repo = XBU::extract_artifacts_from_archive(archive.get(), {"firmware_log.json"});
      
      auto& config_data = artifacts_repo["firmware_log.json"];
      std::string config_content(config_data.begin(), config_data.end());
      
      auto json_config = nlohmann::json::parse(config_content);
      config = smi::firmware_log_config(json_config);
    } 
    catch (const std::exception& e) {
      output << "Error loading firmware log config: " << e.what() << "\n";
      output << "Falling back to raw firmware log data:\n\n";
    }
  }
  
  // Check for watch mode
  if (smi_watch_mode::parse_watch_mode_options(elements_filter)) {
    auto report_generator = [&](const xrt_core::device* dev) -> std::string {
      return (user_wants_raw || !config) 
        ? generate_raw_logs(dev, true)
        : generate_parsed_logs(dev, *config, true);
    };
    
    smi_watch_mode::run_watch_mode(device, output, report_generator);
    return;
  }

  // Non-watch mode
  if (user_wants_raw || !config) {
    output << generate_raw_logs(device, false);
  } else {
    output << "Firmware Log Report\n";
    output << "===================\n\n";
    output << generate_parsed_logs(device, *config, false);
  }
  output << std::endl;
}
