// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportFirmwareLog.h"
#include "FirmwareLogConfig.h"
#include "core/common/query_requests.h"
#include "core/common/time.h"
#include "tools/common/SmiWatchMode.h"
#include "tools/common/Table2D.h"
#include "tools/common/XBUtilities.h"
#include "core/common/json/nlohmann/json.hpp"

#include <algorithm>
#include <boost/format.hpp>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <fstream>
#include <memory>

#define IPU_LOG_FORMAT_FULL 0

using bpt = boost::property_tree::ptree;
namespace XBU = XBUtilities;
namespace smi = xrt_core::tools::xrt_smi;

namespace xrt_core::tools::xrt_smi {

firmware_log_parser::
firmware_log_parser(const firmware_log_config& config) 
  : m_config(config), 
    m_header(m_config.get_log_header()),
    m_header_size(static_cast<uint32_t>(m_config.get_header_size())),
    m_field_indices(create_field_indices(m_config)),
    m_columns({
      {"timestamp", "Timestamp"},
      {"level", "Log-Level"},
      {"appn", "App Number "},
      {"line", "Line Number"},
      {"module", "Module ID"}
    })
  {}

std::unordered_map<std::string, size_t> 
firmware_log_parser::
create_field_indices(const firmware_log_config& config)
{
  std::unordered_map<std::string, size_t> indices;
  const auto& header_struct = config.get_log_header();
  for (size_t i = 0; i < header_struct.fields.size(); ++i) {
    indices[header_struct.fields[i].name] = i;
  }
  return indices;
}

uint64_t 
firmware_log_parser::
extract_value(const uint8_t* data_ptr, 
              size_t byte_offset, 
              size_t bit_offset, 
              size_t bit_width) const
{
  // Read 8 bytes starting from the byte containing our bit field
  uint64_t raw_data = 0;
  size_t start_byte = byte_offset + (bit_offset / bits_per_byte);
  std::memcpy(&raw_data, data_ptr + start_byte, sizeof(uint64_t));
  
  // Extract the field: shift right to align, then mask to width
  size_t shift = bit_offset % bits_per_byte;
  uint64_t mask = (bit_width == bits_per_uint64) ? ~0ULL : ((1ULL << bit_width) - 1);
  return (raw_data >> shift) & mask;
}

std::string 
firmware_log_parser::
format_value(const firmware_log_config::field_info& field, 
             uint64_t value) const
{
  std::string field_value = std::to_string(value);
  
  // Add enum name if field has enumeration
  if (!field.enumeration.empty()) {
    const auto& enums = m_config.get_enums();
    auto itr = enums.find(field.enumeration);
    if (itr != enums.end()) {
      field_value += ":" + itr->second.get_enumerator_name(static_cast<uint32_t>(value));
    }
  }
  return field_value;
}

std::string 
firmware_log_parser::
parse_message(const uint8_t* data_ptr,
              size_t msg_offset,
              size_t buf_size) const
{
  // Always try to read as null-terminated string
  if (msg_offset < buf_size) {
    const char* str_ptr = reinterpret_cast<const char*>(data_ptr + msg_offset);
    // Find the end of the string or buffer boundary
    size_t max_len = buf_size - msg_offset;
    size_t str_len = 0;
    while (str_len < max_len && str_ptr[str_len] != '\0') {
      str_len++;
    }
    if (str_len > 0) {
      std::string message{str_ptr, str_len};
      // Remove trailing newlines
      message.erase(message.find_last_not_of("\n") + 1);
      return message;
    }
  }
  return "";
}

std::vector<std::string> 
firmware_log_parser::
parse_entry(const uint8_t* data_ptr,
            size_t offset,
            size_t buf_size) const
{
  std::vector<std::string> entry_data;
  size_t bit_offset = 0;
  for (const auto& field : m_header.fields) 
  {
    uint64_t value = extract_value(data_ptr, offset, bit_offset, field.width);
    std::string field_value = format_value(field, value);
    entry_data.emplace_back(field_value);
    bit_offset += field.width;
  }
  size_t msg_offset = offset + m_header_size;
  entry_data.emplace_back(parse_message(data_ptr, msg_offset, buf_size));
  return entry_data;
}

std::vector<Table2D::HeaderData> 
firmware_log_parser::
get_table_headers() const
{
  std::vector<Table2D::HeaderData> table_headers;
  
  for (const auto& field : m_header.fields) {
    // Only include fields that are in m_columns
    auto it_map = m_columns.find(field.name);
    if (it_map != m_columns.end()) {
      table_headers.push_back({it_map->second, Table2D::Justification::left});
    }
  }
  table_headers.push_back({"Message", Table2D::Justification::left});
  return table_headers;
}

uint32_t 
firmware_log_parser::
calculate_entry_size(uint32_t argc, uint32_t format) const
{
  uint32_t entry_size = argc;
  if (format == IPU_LOG_FORMAT_FULL) {
    // Firmware uses 8-byte alignment to optimize DMA transfers and memory operations.
    // Each log argument is 4 bytes, so argc*4 = total argument payload size.
    // Round up to next 8-byte boundary: ((size + 7) / 8) * 8
    entry_size = ((static_cast<size_t>(argc) * 4 + byte_alignment) / bits_per_byte) * bits_per_byte + m_header_size; 
  } else {
    // Concise format: firmware writes byte-by-byte for minimal storage
    entry_size = argc + m_header_size;
  }
  return entry_size;
}

Table2D 
firmware_log_parser::
parse(const uint8_t* data_ptr, size_t buf_size) const
{
  Table2D log_table(get_table_headers());
  size_t offset = 0;

  while (offset + m_header_size <= buf_size) {
    auto entry_data = parse_entry(data_ptr, offset, buf_size);
    
    auto format = std::stoul(entry_data[m_field_indices.at("format")]); 
    auto argc = std::stoul(entry_data[m_field_indices.at("argc")]);
    
    // Only include fields that are in m_columns header fields
    std::vector<std::string> log_data;
    for (const auto& field : m_header.fields) {
      if (m_columns.find(field.name) != m_columns.end()) {
        log_data.emplace_back(std::move(entry_data[m_field_indices.at(field.name)]));
      }
    }
    // Add message
    log_data.emplace_back(std::move(entry_data.back())); 
    log_table.addEntry(std::move(log_data));

    offset += calculate_entry_size(argc, format);
  }
  return log_table;
}

static std::string
generate_raw_logs(const xrt_core::device* dev,
                  bool is_watch,
                  uint64_t& watch_mode_offset)
{
  std::stringstream ss{};

  try {
    // Create and setup buffer for firmware log data
    smi_debug_buffer debug_buf(watch_mode_offset, is_watch);

    // Get raw buffer from device for raw dump
    xrt_core::device_query<xrt_core::query::firmware_log_data>(dev, debug_buf.get_log_buffer());
    
    watch_mode_offset = debug_buf.get_log_buffer().abs_offset;
    
    if (!debug_buf.get_log_buffer().data) {
      ss << "No firmware log data available\n";
      return ss.str();
    }

    const auto* data_ptr = static_cast<const uint8_t*>(debug_buf.get_log_buffer().data);
    size_t buf_size = debug_buf.get_log_buffer().size;

    // Simply print the raw payload data
    ss.write(reinterpret_cast<const char*>(data_ptr), buf_size);
  } catch (const std::exception& e) {
    ss << "Error retrieving raw firmware log data: " << e.what() << "\n";
  }

  return ss.str();
}

static std::string
generate_firmware_log_report(const xrt_core::device* dev,
                             const firmware_log_config& config,
                             bool is_watch)
{
  std::stringstream ss{};
  static uint64_t watch_mode_offset = 0;

  // Create and setup buffer for firmware log data
  smi_debug_buffer debug_buf(watch_mode_offset, is_watch);

  // Get buffer from driver
  xrt_core::device_query<xrt_core::query::firmware_log_data>(dev, debug_buf.get_log_buffer());
  
  watch_mode_offset = debug_buf.get_offset();
  
  if (!debug_buf.get_log_buffer().data) {
    ss << "No firmware log data available\n";
    return ss.str();
  }

  // Create parser instance and parse the firmware log buffer
  firmware_log_parser parser(config);
  const auto* data_ptr = static_cast<const uint8_t*>(debug_buf.get_data());
  size_t buf_size = debug_buf.get_size();
  
  Table2D log_table = parser.parse(data_ptr, buf_size);
  ss << "  Firmware Log Information:\n";
  ss << log_table.toString("    ");
  return ss.str();
}

} // namespace xrt_core::tools::xrt_smi

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
  try {
    auto archive = XBU::open_archive(device);
    auto artifacts_repo = XBU::extract_artifacts_from_archive(archive.get(), {"firmware_log.json"});
    
    auto& config_data = artifacts_repo["firmware_log.json"];
    std::string config_content(config_data.begin(), config_data.end());
    
    auto json_config = nlohmann::json::parse(config_content);
    smi::firmware_log_config config(json_config);
    
    // Check for watch mode
    if (smi_watch_mode::parse_watch_mode_options(elements_filter)) {
      auto report_generator = [config](const xrt_core::device* dev) -> std::string {
        return smi::generate_firmware_log_report(dev, config, true);
      };
      smi_watch_mode::run_watch_mode(device, output, report_generator, "Firmware Log");
      return;
    }

    // Non-watch mode
    output << "Firmware Log Report\n";
    output << "===================\n\n";
    output << xrt_core::tools::xrt_smi::generate_firmware_log_report(device, config, false);
  } 
  catch (const std::exception& e) {
    output << "Error loading config from archive: " << e.what() << "\n";
    output << "Falling back to raw firmware log data:\n\n";
    
    // Generate raw logs when config is not available
    static uint64_t watch_mode_offset = 0;
    if (smi_watch_mode::parse_watch_mode_options(elements_filter)) {
      // Create report generator lambda for watch mode with raw logs
      auto report_generator = [](const xrt_core::device* dev) -> std::string {
        return smi::generate_raw_logs(dev, true, watch_mode_offset);
      };

      smi_watch_mode::run_watch_mode(device, output, report_generator, "Event Trace (Raw)");
      return;
    }
    output << smi::generate_raw_logs(device, false, watch_mode_offset);
    return;
  }
  output << std::endl;
}


