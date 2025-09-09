// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "core/common/query_requests.h"
#include "core/common/time.h"
#include "core/common/module_loader.h"
#include "tools/common/SmiWatchMode.h"
#include "tools/common/Table2D.h"
#include "EventTraceConfig.h"
#include "ReportEventTrace.h"

// 3rd Party Library - Include Files
#include <algorithm>
#include <boost/format.hpp>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>
#include <cstring>

using bpt = boost::property_tree::ptree;

// Event trace data structure (must match device.cpp implementation)
struct trace_event {
  uint64_t timestamp;    // Simulated timestamp
  uint16_t event_id;     // Event ID from trace_events.h
  uint64_t payload;      // Event payload/arguments
};

// Global event trace configuration instance
static xrt_core::tools::xrt_smi::event_trace_config* 
get_event_trace_config(const xrt_core::device* dev) {

  boost::property_tree::ptree ptree;
  std::string config = xrt_core::device_query<xrt_core::query::event_trace_config>(dev);

  static xrt_core::tools::xrt_smi::event_trace_config config_obj(config);
  return &config_obj;
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
    auto config = get_event_trace_config(dev);
    
    // Query version information from device using specific query struct (like telemetry)
    auto version_info = xrt_core::device_query<xrt_core::query::event_trace_version>(dev);
    
    event_trace_pt.put("device_version_major", version_info.major);
    event_trace_pt.put("device_version_minor", version_info.minor);
    
    // Query event trace data from device using specific query struct (like telemetry)
    auto log_buffer = xrt_core::device_query<xrt_core::query::event_trace_data>(dev);

    // Parse trace events from buffer
    if (log_buffer.data && log_buffer.size > 0) {
      size_t event_count = log_buffer.size / sizeof(trace_event);
      auto events = static_cast<trace_event*>(log_buffer.data);
      
      bpt events_array{};
      for (size_t i = 0; i < event_count; ++i) {
        // Parse event using json based configuration
        xrt_core::tools::xrt_smi::event_record record{events[i].timestamp, 
                            events[i].event_id, 
                            events[i].payload};

        auto parsed_event = config->parse_event(record);

        bpt event_pt;
        event_pt.put("timestamp", parsed_event.timestamp);
        event_pt.put("event_id", parsed_event.event_id);
        event_pt.put("event_name", parsed_event.name);
        
        // Join categories with pipe separator for backward compatibility
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
      event_trace_pt.put("buffer_offset", log_buffer.abs_offset);
      event_trace_pt.put("buffer_size", log_buffer.size);
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

static void
validate_version_compatibility(const std::pair<uint16_t, uint16_t>& version,
                               const xrt_core::device* device) 
{
  if (!device) {
    throw std::runtime_error("Warning: Cannot validate event trace version - no device provided");
  }

  auto firmware_version = xrt_core::device_query<xrt_core::query::event_trace_version>(device);
  if (version.first != firmware_version.major || version.second != firmware_version.minor) {
    std::stringstream err;
    err << "Warning: Event trace version mismatch!\n"
        << "  JSON file version: " << version.first << "." << version.second << "\n"
        << "  Firmware version: " << firmware_version.major << "." << firmware_version.minor << "\n"
        << "  Event parsing may be incorrect or incomplete.";
    throw std::runtime_error(err.str());
  }
}

static std::string
generate_event_trace_report(const xrt_core::device* dev,
                            bool is_watch)
{
  std::stringstream ss{};
  
  try {
    // Get the event trace configuration
    auto config = get_event_trace_config(dev);

    auto version = config->get_file_version();
    validate_version_compatibility(version, dev);

    // Query event trace data from device using specific query struct
    auto log_buffer = xrt_core::device_query<xrt_core::query::event_trace_data>(dev, is_watch);
    
    ss << boost::format("Event Trace Report (Buffer: %d bytes) - %s\n") 
          % log_buffer.size % xrt_core::timestamp();
    ss << "=======================================================\n";

    ss << "\n";

    if (!log_buffer.data || log_buffer.size == 0) {
      ss << "No event trace data available\n";
      return ss.str();
    }

    // Parse trace events from buffer
    size_t event_count = log_buffer.size / sizeof(trace_event);
    auto events = static_cast<trace_event*>(log_buffer.data);

    ss << boost::format("Total Events: %d\n") % event_count;
    ss << boost::format("Buffer Offset: %d\n\n") % log_buffer.abs_offset;

    // Create Table2D with headers (enhanced with parsed args)
    const std::vector<Table2D::HeaderData> table_headers = {
      {"Timestamp",         Table2D::Justification::right},
      {"Event ID",          Table2D::Justification::left},
      {"Event Name",        Table2D::Justification::left},
      {"Category",          Table2D::Justification::left},
      {"Payload",           Table2D::Justification::left},
      {"Parsed Args",       Table2D::Justification::left}
    };
    Table2D event_table(table_headers);
    
    // Add data rows
    for (size_t i = 0; i < event_count; ++i) {
      // Parse event using JSON-based configuration
      xrt_core::tools::xrt_smi::event_record record{events[i].timestamp, events[i].event_id, events[i].payload};
      auto parsed_event = config->parse_event(record);

      // Join categories with pipe separator for backward compatibility
      std::string categories_str{};
      for (size_t j = 0; j < parsed_event.categories.size(); ++j) {
        if (j > 0) categories_str += "|";
        categories_str += parsed_event.categories[j];
      }
      
      // Format parsed arguments
      std::string args_str{};
      for (const auto& arg_pair : parsed_event.args) {
        if (!args_str.empty()) args_str += ", ";
        args_str += arg_pair.first + "=" + arg_pair.second;
      }
      
      const std::vector<std::string> entry_data = {
        std::to_string(parsed_event.timestamp),
        (boost::format("0x%04x") % parsed_event.event_id).str(),
        parsed_event.name,
        categories_str,
        (boost::format("0x%x") % parsed_event.raw_payload).str(),
        args_str
      };
      event_table.addEntry(entry_data);
    }

    ss << "  Event Trace Information:\n";
    ss << event_table.toString("    ");

  } 
  catch (const std::exception& e) {
    ss << "Error retrieving event trace data: " << e.what() << "\n";
  }

  return ss.str();
}

void
ReportEventTrace::
writeReport(const xrt_core::device* device,
            const bpt& /*pt*/,
            const std::vector<std::string>& elements_filter,
            std::ostream& output) const
{
  // Check for watch mode
  if (smi_watch_mode::parse_watch_mode_options(elements_filter)) {
    // Create report generator lambda for watch mode
    auto report_generator = [](const xrt_core::device* dev) -> std::string {
      return generate_event_trace_report(dev, true); 
    };

    smi_watch_mode::run_watch_mode(device, output, report_generator, "Event Trace");
    return;
  }
  output << "Event Trace Report\n";
  output << "==================\n\n";
  output << generate_event_trace_report(device, false);
  output << std::endl;
}
