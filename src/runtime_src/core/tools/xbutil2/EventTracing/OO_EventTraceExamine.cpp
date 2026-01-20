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
#include "../OutputStreamHelper.h"

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
    : OptionOptions(_longName, _isHidden, "Status|watch event trace data")
    , m_device("")
    , m_help(false)
    , m_watch(false)
    , m_raw(std::nullopt)
    , m_version(false)
    , m_status(false)
    , m_watch_mode_offset(0)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("status", boost::program_options::bool_switch(&m_status), "Show event trace status")
    ("watch", boost::program_options::bool_switch(&m_watch), "Watch event trace data continuously")
    ("raw", boost::program_options::value<std::string>()->notifier([this](const std::string& value) { m_raw = value; })->implicit_value(""),
            "Output raw event trace data (no parsing). Optionally specify output file. Default is to output to console.")
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
    throw xrt_core::error(std::errc::operation_canceled, "Error getting payload version: " + std::string(e.what()));
  }
}

void
OO_EventTraceExamine::
handle_status(const xrt_core::device* device) const {
  try {
    auto status = xrt_core::device_query<xrt_core::query::event_trace_state>(device);
    std::cout << "Event trace status: " << (status.action == 1 ? "enabled" : "disabled") << "\n"; //NOLINT
    
    auto category_names = smi::event_trace_config::mask_to_category_names(status.categories, device);
    if (!category_names.empty()) {
      std::cout << "Event trace categories: ";
      for (size_t i = 0; i < category_names.size(); ++i) { //NOLINT
        if (i > 0) std::cout << ", "; //NOLINT
        std::cout << category_names[i];
      }
      std::cout << "\n";
    } else {
      std::cout << "Event trace categories: none\n";
    }
  } catch (const std::exception& e) {
    throw xrt_core::error(std::errc::operation_canceled, 
        "Error getting event trace status: " + std::string(e.what()) + "\n" +
        "Use 'xbutil examine --help' for more information.");
  }
}

void
OO_EventTraceExamine::
handle_logging(const xrt_core::device* device) const {
  // Use helper to manage output stream
  XBUtilities::OutputStreamHelper output_helper(m_raw);
  std::ostream& out = output_helper.get_stream();

  // Load configuration unless raw mode is requested
  std::unique_ptr<smi::event_trace_config> config;
  if (!output_helper.is_raw_mode()) {
    try {
      config = smi::event_trace_config::create_from_device(device);
    } catch (const std::exception& e) {
      out << "[Error] Configuration loading failed: " << e.what() << std::endl;
      return;
    }
  }

  // Handle raw mode version display
  if (output_helper.is_raw_mode()) {
    dump_raw_version(device);
  }

  if (m_watch) {
    if (!output_helper.is_raw_mode() && config) {
      auto parser = smi::event_trace_parser::create_from_config(config, device);
      out << add_header();
      
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
      out << "Event Trace Logs\n";
      out << "==================\n\n";
      out << add_header();
      
      auto parser = smi::event_trace_parser::create_from_config(config, device);
      out << generate_parsed_logs(device, parser, false);
    }
  }
  // output_helper destructor automatically flushes and closes the file
}

std::string
OO_EventTraceExamine::
generate_parsed_logs(const xrt_core::device* device,
                    const std::unique_ptr<smi::event_trace_parser>& parser,
                    bool is_watch) const
{
  std::stringstream ss{};
  
  try {
    smi_debug_buffer debug_buf(m_watch_mode_offset, is_watch);
    xrt_core::query::firmware_debug_buffer data_buf{};
    data_buf = xrt_core::device_query<xrt_core::query::event_trace_data>(device, debug_buf.get_log_buffer());
    
    m_watch_mode_offset = data_buf.abs_offset;

    if (!data_buf.data)
      return ss.str();

    auto* data_ptr = static_cast<const uint8_t*>(data_buf.data);
    auto buf_size = data_buf.size;
    ss << parser->parse(data_ptr, buf_size);
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
  XBUtilities::sudo_or_throw("Event tracing requires admin privileges");

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
    throw xrt_core::error(std::errc::operation_canceled, ex.what());
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
    throw xrt_core::error(std::errc::operation_canceled, "ERROR: " + std::string(e.what()));
  }

  if (m_version) {
    handle_version(device.get());
    return;
  }

  if (m_status) {
    handle_status(device.get());
    return;
  }

  // Handle watch mode or default dump action
  handle_logging(device.get());
}