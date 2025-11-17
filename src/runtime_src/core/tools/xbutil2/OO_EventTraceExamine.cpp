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
#include <iomanip>
#include <sstream>
#include <memory>
#include <optional>
#include <stdexcept>

namespace po = boost::program_options;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_EventTraceExamine::OO_EventTraceExamine( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Watch event trace data")
    , m_device("")
    , m_help(false)
    , m_watch(false)
    , m_raw(false)
    , m_version(false)
    , m_watch_mode_offset(0)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("watch", boost::program_options::bool_switch(&m_watch), "Watch event trace data continuously")
    ("raw", boost::program_options::bool_switch(&m_raw), "Output raw event trace data (no parsing)")
    ("payload-version", boost::program_options::bool_switch(&m_version), "Show event trace version")
  ;
}

void
OO_EventTraceExamine::
handle_version(const xrt_core::device* device) const {
  try {
    // Get event trace version
    auto version = xrt_core::device_query<xrt_core::query::event_trace_version>(device);
    
    // Extract version components from 32-bit integer
    // Format: [product][schema][major][minor] - one byte each
    //NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    uint8_t product = (version >> 24) & 0xFF;
    uint8_t schema = (version >> 16) & 0xFF;
    uint8_t major = (version >> 8) & 0xFF;
    uint8_t minor = version & 0xFF;
    //NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
    
    std::cout << boost::format("  %-20s : %u\n") % "Product" % static_cast<unsigned>(product);
    std::cout << boost::format("  %-20s : %u\n") % "Schema" % static_cast<unsigned>(schema);
    std::cout << boost::format("  %-20s : %u\n") % "Major" % static_cast<unsigned>(major);
    std::cout << boost::format("  %-20s : %u\n") % "Minor" % static_cast<unsigned>(minor);
  } catch (const std::exception& e) {
    std::cerr << "Error getting payload version: " << e.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

void
OO_EventTraceExamine::
handle_logging(const xrt_core::device* device) const {
  // Load configuration unless raw mode is requested
  std::optional<smi::event_trace_config> config;
  if (!m_raw) {
    try {
      config = smi::event_trace_config::load_config(device);
    } catch (const std::exception& e) {
      std::cerr << "[Error] Configuration loading failed: " << e.what() << std::endl;
      return;
    }
  }

  // Handle raw mode version display
  if (m_raw) {
    dump_raw_version(device);
  }

  if (m_watch) {
    if (!m_raw && config) {
      smi::event_trace_parser parser(*config);
      std::cout << add_header();
      
      auto report_generator = [this, &parser](const xrt_core::device* dev) -> std::string {
        return generate_parsed_logs(dev, parser, true);
      };
      smi_watch_mode::run_watch_mode(device, std::cout, report_generator);
    } else {
      // Raw mode: no parser needed
      auto report_generator = [this](const xrt_core::device* dev) -> std::string {
        return generate_raw_logs(dev, true);
      };
      smi_watch_mode::run_watch_mode(device, std::cout, report_generator);
    }
  } else {
    if (m_raw || !config) {
      std::cout << generate_raw_logs(device, false);
    } else {
      std::cout << "Event Trace Logs\n";
      std::cout << "==================\n\n";
      std::cout << add_header();
      
      smi::event_trace_parser parser(*config);
      std::cout << generate_parsed_logs(device, parser, false);
    }
  }
}

std::string
OO_EventTraceExamine::
generate_parsed_logs(const xrt_core::device* dev,
                    const smi::event_trace_parser& parser,
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

std::string
OO_EventTraceExamine::
add_header() const
{
  std::stringstream ss{};
  
  // Format table header with proper spacing
  ss << boost::format("%-20s %-25s %-25s %-30s\n") //NOLINT (cppcoreguidelines-avoid-magic-numbers)
        % "Timestamp" 
        % "Event Name" 
        % "Category" 
        % "Arguments";
  
  return ss.str();
}

void
OO_EventTraceExamine::
dump_raw_version(const xrt_core::device* device) const
{
  auto version = xrt_core::device_query<xrt_core::query::event_trace_version>(device);
  
  // Cast version to char* and dump the raw bytes
  std::string version_str(reinterpret_cast<const char*>(&version), sizeof(version));
  std::cout << version_str;
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

  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBUtilities::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if (m_version) {
    handle_version(device.get());
    return;
  }

  // Handle watch mode or default dump action
  handle_logging(device.get());
}