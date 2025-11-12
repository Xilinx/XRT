// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_EventTraceExamine.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBHelpMenusCore.h"
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

OO_EventTraceExamine::OO_EventTraceExamine( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Status|watch event trace data")
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
    ("status", boost::program_options::bool_switch(&m_status), "Show event trace status")
    ("watch", boost::program_options::bool_switch(&m_watch), "Watch event trace data continuously")
    ("raw", boost::program_options::bool_switch(&m_raw), "Output raw event trace data (no parsing)")
  ;

  m_positionalOptions.
    add("status", 1 /* max_count */).
    add("watch", 1 /* max_count */)
  ;
}

void
OO_EventTraceExamine::
validate_args() const {
  // Default behavior is to dump event trace data once
  // Only explicit actions are --status and --watch
  if(m_status && m_watch)
    throw xrt_core::error(std::errc::operation_canceled, "Cannot specify both --status and --watch");
}

std::string
OO_EventTraceExamine::
generate_parsed_logs(const xrt_core::device* dev,
                    const smi::event_trace_config& config,
                    bool is_watch) const
{
  std::stringstream ss{};
  
  try {
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
    auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
    auto buf_size = data_buf.size;

    ss << parser.parse(data_ptr, buf_size);
  } 
  catch (const std::exception& e) {
    ss << "Error retrieving event trace data: " << e.what() << "\n";
    m_watch_mode_offset = 0;
  }
  
  return ss.str();
}

std::string
OO_EventTraceExamine::
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
    auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
    
    ss.write(reinterpret_cast<const char*>(data_ptr), static_cast<std::streamsize>(data_buf.size));
  } 
  catch (const std::exception& e) {
    ss << "Error retrieving raw event trace data: " << e.what() << "\n";
  }
  return ss.str();
}

void
OO_EventTraceExamine::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Event Trace Examine");

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
      auto status = xrt_core::device_query<xrt_core::query::event_trace_state>(device.get());
      std::cout << "Event trace status: " << (status.action == 1 ? "enabled" : "disabled") << "\n";
      std::cout << "Event trace categories: " << status.categories << "\n";
    } catch (const std::exception& e) {
      std::cerr << "Error getting event trace status: " << e.what() << "\n";
      throw xrt_core::error(std::errc::operation_canceled);
    }
    return;
  }

  // Handle watch or default dump action
  bool is_watch = m_watch;
  
  // Try to parse config unless user explicitly wants raw logs
  std::optional<smi::event_trace_config> config;

  if (!m_raw) {
    config = smi::event_trace_config::load_config(device.get());
    if (!config) {
      std::cout << "Warning: Dumping raw event trace: Failed to load configuration\n";
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
      std::cout << "Event Trace Report\n";
      std::cout << "==================\n\n";
      std::cout << generate_parsed_logs(device.get(), *config, false);
    }
  }
}