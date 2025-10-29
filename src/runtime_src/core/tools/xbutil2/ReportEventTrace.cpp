// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "core/common/time.h"
#include "core/common/module_loader.h"
#include "tools/common/SmiWatchMode.h"
#include "tools/common/XBUtilities.h"
#include "ReportEventTrace.h"

// 3rd Party Library - Include Files
#include <algorithm>
#include <boost/format.hpp>
#include <cstring>
#include <utility>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>
#include <cstring>
#include "core/common/json/nlohmann/json.hpp"

using bpt = boost::property_tree::ptree;
namespace XBU = XBUtilities;

std::string
ReportEventTrace::
generate_raw_logs(const xrt_core::device* dev,
                  bool is_watch) const
{
  std::stringstream ss{};

  try {
    smi_debug_buffer debug_buf(m_watch_mode_offset, is_watch);
    auto data_buf = xrt_core::device_query<xrt_core::query::event_trace_data>(dev, debug_buf.get_log_buffer());
    
    m_watch_mode_offset = data_buf.abs_offset;
    
    if (!data_buf.data || data_buf.size == 0) {
      ss << "No event trace data available\n";
      return ss.str();
    }

    // Simply print the raw payload data
    const auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
    auto buf_size = data_buf.size;
    
    ss.write(reinterpret_cast<const char*>(data_ptr), buf_size);
  } 
  catch (const std::exception& e) {
    ss << "Error retrieving raw event trace data: " << e.what() << "\n";
  }
  return ss.str();
}

std::string
ReportEventTrace::
generate_parsed_logs(const xrt_core::device* dev,
                     const smi::event_trace_config& config,
                     bool is_watch) const
{
  std::stringstream ss{};
  
  try {
    auto version = config.get_file_version();
    validate_version_compatibility(version, dev);

    smi_debug_buffer debug_buf(m_watch_mode_offset, is_watch);
    xrt_core::query::firmware_debug_buffer data_buf{};
    data_buf = xrt_core::device_query<xrt_core::query::event_trace_data>(dev, debug_buf.get_log_buffer());
    
    m_watch_mode_offset = data_buf.abs_offset;

    if (!data_buf.data) {
      ss << "No event trace data available\n";
      return ss.str();
    }

    // Create parser instance and parse the event trace buffer directly to string
    smi::event_trace_parser parser(config);
    const auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
    auto buf_size = data_buf.size;

    ss << parser.parse(data_ptr, buf_size);
  } 
  catch (const std::exception& e) {
    ss << "Error retrieving event trace data: " << e.what() << "\n";
    m_watch_mode_offset = 0;
  }
  
  return ss.str();
}

void
ReportEventTrace::
validate_version_compatibility(const std::pair<uint16_t, uint16_t>& /*version*/,
                               const xrt_core::device* device) const 
{
  if (!device) {
    throw std::runtime_error("Warning: Cannot validate event trace version - no device provided");
  }

  /*
  TODO : Add version logic based on what driver provides
  auto firmware_version = xrt_core::device_query<xrt_core::query::event_trace_version>(device);
  if (version.first != firmware_version.major || version.second != firmware_version.minor) {
    std::stringstream err{};
    err << "Warning: Event trace version mismatch!\n"
        << "  JSON file version: " << version.first << "." << version.second << "\n"
        << "  Firmware version: " << firmware_version.major << "." << firmware_version.minor << "\n"
        << "  Event parsing may be incorrect or incomplete.";
    throw std::runtime_error(err.str());
  }
  */
}

void
ReportEventTrace::
getPropertyTreeInternal(const xrt_core::device* dev, bpt& pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void
ReportEventTrace::
getPropertyTree20202(const xrt_core::device* dev, bpt& pt) const
{
  bpt event_trace_pt{};

  try {
    // Get the event trace configuration
    auto archive = XBU::open_archive(dev);
    auto artifacts_repo = XBU::extract_artifacts_from_archive(archive.get(), {"trace_events.json"});
    
    auto& config_data = artifacts_repo["trace_events.json"];
    std::string config_content(config_data.begin(), config_data.end());
    
    nlohmann::json json_config = nlohmann::json::parse(config_content);
    auto config = smi::event_trace_config(json_config);
    
    smi_debug_buffer debug_buf(0, false);
    
    // Query event trace data from device using specific query struct (like telemetry)
    auto data_buf = xrt_core::device_query<xrt_core::query::event_trace_data>(dev, debug_buf.get_log_buffer());

    // Parse trace events from buffer
    if (data_buf.data && data_buf.size > 0) {
      // Calculate total event size from config
      size_t total_event_size = config.get_event_size();
      size_t event_count = data_buf.size / total_event_size;

      bpt events_array{};
      const auto* current_ptr = static_cast<const uint8_t*>(data_buf.data);
      
      for (size_t i = 0; i < event_count; ++i) {
        // Parse event from buffer and decode using json based configuration
        auto parsed_event = config.decode_event(config.parse_buffer(current_ptr));
        current_ptr += total_event_size;

        bpt event_pt;
        event_pt.put("timestamp", parsed_event.timestamp);
        event_pt.put("event_id", parsed_event.event_id);
        event_pt.put("event_name", parsed_event.name);
        
        std::string categories_str{};
        for (size_t j = 0; j < parsed_event.categories.size(); ++j) {
          if (j > 0) categories_str += "|";
          categories_str += parsed_event.categories[j];
        }
        event_pt.put("category", categories_str);
        
        event_pt.put("payload", parsed_event.raw_payload);
        
        // Add parsed arguments
        bpt args_pt;
        for (const auto& arg_pair : parsed_event.args) {
          args_pt.put(arg_pair.first, arg_pair.second);
        }
        if (!parsed_event.args.empty()) {
          event_pt.add_child("args", args_pt);
        }
        events_array.push_back(std::make_pair("", event_pt));
      }
      event_trace_pt.add_child("events", events_array);
      event_trace_pt.put("event_count", event_count);
      event_trace_pt.put("buffer_offset", debug_buf.get_offset());
      event_trace_pt.put("buffer_size", debug_buf.get_size());
    } else {
      event_trace_pt.put("event_count", 0);
      event_trace_pt.put("buffer_offset", 0);
      event_trace_pt.put("buffer_size", 0);
    }
  } 
  catch (const std::exception& e) {
    event_trace_pt.put("event_count", 0);
    event_trace_pt.put("error", e.what());
  }

  // There can only be 1 root node
  pt.add_child("event_trace", event_trace_pt);
}


void
ReportEventTrace::
writeReport(const xrt_core::device* device,
            const bpt& /*pt*/,
            const std::vector<std::string>& elements_filter,
            std::ostream& output) const
{
  bool user_wants_raw = std::find(elements_filter.begin(), elements_filter.end(), "raw") != elements_filter.end();
  
  if (std::find(elements_filter.begin(), elements_filter.end(), "status") != elements_filter.end()) {
    auto status = xrt_core::device_query<xrt_core::query::event_trace_state>(device);
    output << "Event trace status: "<<(status.action == 1 ? "enabled" : "disabled");
    output << "Event trace categories: "<< status.categories;
    return;
  }
  // Try to parse config unless user explicitly wants raw logs
  std::optional<smi::event_trace_config> config;

  if (!user_wants_raw) {
    try {
      auto archive = XBU::open_archive(device);
      if (!archive) {
        throw std::runtime_error("Failed to open archive");
      }
      auto artifacts_repo = XBU::extract_artifacts_from_archive(archive.get(), {"trace_events.json"});
      
      auto& config_data = artifacts_repo["trace_events.json"];
      std::string config_content(config_data.begin(), config_data.end());
      
      auto json_config = nlohmann::json::parse(config_content);
      config = smi::event_trace_config(json_config);
    } 
    catch (const std::exception& e) {
      output << "Warning : Dumping raw event trace data: " << e.what() << "\n";
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
    output << "Event Trace Report\n";
    output << "==================\n\n";
    output << generate_parsed_logs(device, *config, false);
  }
  output << std::endl;
}
