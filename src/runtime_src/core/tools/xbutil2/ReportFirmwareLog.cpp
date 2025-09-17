// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportFirmwareLog.h"
#include "FirmwareLogConfig.h"
#include "core/common/query_requests.h"
#include "core/common/time.h"
#include "tools/common/SmiWatchMode.h"
#include "tools/common/Table2D.h"

#include <algorithm>
#include <boost/format.hpp>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cstring>

using bpt = boost::property_tree::ptree;

namespace xrt_core::tools::xrt_smi {

// Function to generate structured dummy firmware log data matching ipu_log_message_header format
static void generate_dummy_firmware_log_data(xrt_core::query::firmware_debug_buffer& log_buffer) {
  static uint64_t counter = 0;  // Ever-increasing counter
  counter++;
  
  auto data_ptr = static_cast<uint8_t*>(log_buffer.data);
  size_t total_size = 0;
  
  // Generate 5 entries for testing
  size_t num_entries = 5; // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
  
  for (size_t entry = 0; entry < num_entries; ++entry) {
    // Random timestamp with some ordering
    uint64_t timestamp = 1000000000ULL + counter * 10000 + entry * 100; // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    
    // Random format (0 or 1)
    uint32_t format = (counter + entry) % 2; // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    
    // Random log level (0-7)
    uint32_t level = (counter + entry) % 8; // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    
    // Random application number (1-8)
    uint32_t appn = 1 + ((counter + entry) % 8); // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    
    // Random line number (1-8)
    uint32_t line = 1 + ((counter + entry) % 8); // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    
    // Random module ID (1-15)
    uint32_t module = 1 + ((counter + entry * 3) % 15); // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    
    // Generate string messages instead of hex payloads
    std::vector<std::string> dummy_messages = {
      "System initialization complete",
      "DMA transfer started",
      "Command queue overflow warning", 
      "Temperature threshold exceeded",
      "Memory allocation failed"
    };
    
    std::string message = dummy_messages[entry % dummy_messages.size()]; // NOLINT(readability-magic-numbers) - dummy data for pretty printing
    
    // Set argc to 0 since we're using string messages, not hex arguments
    uint32_t argc = 0;
    
    // Pack all bit fields into a single 64-bit value
    uint64_t packed = (static_cast<uint64_t>(format) << 0) | // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
                      (static_cast<uint64_t>(0x60) << 1) | // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
                      (static_cast<uint64_t>(level) << 8) | // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
                      (static_cast<uint64_t>(0) << 11) | // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
                      (static_cast<uint64_t>(appn) << 16) | // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
                      (static_cast<uint64_t>(argc) << 24) |// NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
                      (static_cast<uint64_t>(line) << 32) |// NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
                      (static_cast<uint64_t>(module) << 48); // NOLINT(cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    
    // Write header (16 bytes total)
    memcpy(data_ptr + total_size, &timestamp, 8); // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    total_size += 8; // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    memcpy(data_ptr + total_size, &packed, 8); // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    total_size += 8; // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
    
    // Write string message as null-terminated string
    size_t msg_len = message.length() + 1; // +1 for null terminator
    memcpy(data_ptr + total_size, message.c_str(), msg_len);
    total_size += msg_len;
    
    // Align to 4-byte boundary
    while (total_size % 4 != 0) { // NOLINT(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers) - dummy data for pretty printing
      data_ptr[total_size++] = 0;
    }
  }
  
  log_buffer.size = total_size;
  log_buffer.abs_offset = counter * total_size;  // Ever-increasing offset
}

struct field_context {
  const uint8_t* data_ptr;
  size_t offset;
  size_t bit_offset;
  size_t bit_width;
  std::shared_ptr<firmware_log_config::field_info> field;
};

static std::string
parse_field(const field_context& ctx,
            const firmware_log_config& config)
{
  size_t byte_offset = ctx.offset + (ctx.bit_offset / bits_per_byte);
  size_t end_bit = ctx.bit_offset + ctx.bit_width;
  size_t end_byte = ctx.offset + ((end_bit + byte_alignment) / bits_per_byte);
  uint64_t raw = 0;
  size_t bytes_to_read = end_byte - byte_offset;
  std::memcpy(&raw, ctx.data_ptr + byte_offset, bytes_to_read);
  size_t lsb = ctx.bit_offset % bits_per_byte;
  uint64_t mask = (ctx.bit_width == bits_per_uint64) ? ~0ULL : ((1ULL << ctx.bit_width) - 1);
  uint64_t value = (raw >> lsb) & mask;

  // For dummy data, display decimal values instead of hex
  if (ctx.field && ctx.field->name == "level") {
    const auto& enums = config.get_enums();
    auto itr = enums.find(ctx.field->enumeration);
    if (itr != enums.end()) {
      return std::to_string(value) + ":" + itr->second.get_enumerator_name(static_cast<uint32_t>(value));
    }
  }
  return std::to_string(value);
}

static std::string
parse_message(const uint8_t* data_ptr,
              size_t msg_offset,
              size_t argc,
              size_t buf_size)
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
      return {str_ptr, str_len};
    }
  }
  return "";
}

static std::vector<std::string> 
parse_log_entry(const uint8_t* data_ptr,
                size_t offset,
                size_t buf_size,
                const firmware_log_config& config)
{
  uint32_t argc = 0;
  size_t header_size = config.get_header_size();
  const auto& structures = config.get_structures();
  auto it = structures.find("ipu_log_message_header");
  if (it == structures.end()) 
  {
    return {}; // Handle error: structure not found
  }

  std::vector<std::string> entry_data;
  size_t bit_offset = 0;
  for (const auto& field : it->second.fields) 
  {
    size_t bit_width = field.width;
    size_t end_bit = bit_offset + bit_width;
    if (offset + ((end_bit + byte_alignment) / bits_per_byte) <= offset + header_size) {
      field_context ctx{data_ptr, offset, bit_offset, bit_width, std::make_shared<firmware_log_config::field_info>(field)};
      std::string field_value = parse_field(ctx, config);
      if (field.name == "argc") {
        argc = static_cast<uint32_t>(std::stoul(field_value, nullptr, hex_width_64));
      }
      
      // Skip reserved fields when adding to entry data
      if (field.name.find("reserved") == std::string::npos) {
        entry_data.emplace_back(field_value);
      }
    } else {
      // Skip reserved fields when adding error placeholder
      if (field.name.find("reserved") == std::string::npos) {
        entry_data.emplace_back("-");
      }
    }
    bit_offset += bit_width;
  }

  size_t msg_offset = offset + header_size;
  entry_data.emplace_back(parse_message(data_ptr, msg_offset, argc, buf_size));
  return entry_data;
}

static std::string
generate_raw_logs(const xrt_core::device* dev,
                  bool is_watch,
                  uint64_t& watch_mode_offset)
{
  std::stringstream ss{};

  try {
    // Create and setup buffer for firmware log data
    std::vector<char> buffer;
    xrt_core::query::firmware_debug_buffer log_buffer{};
    smi_watch_mode::setup_debug_buffer(buffer, log_buffer, watch_mode_offset, is_watch);

    // Get raw buffer from device for raw dump
    xrt_core::device_query<xrt_core::query::firmware_log_data>(dev, log_buffer);
    
    watch_mode_offset = log_buffer.abs_offset;
    
    if (!log_buffer.data) {
      ss << "No firmware log data available\n";
      return ss.str();
    }

    const auto* data_ptr = static_cast<const uint8_t*>(log_buffer.data);
    size_t buf_size = log_buffer.size;

    // Simply print the raw payload data
    ss.write(reinterpret_cast<const char*>(data_ptr), buf_size);
  } catch (const std::exception& e) {
    ss << "Error retrieving raw firmware log data: " << e.what() << "\n";
  }

  return ss.str();
}

static std::string
generate_firmware_log_report(const xrt_core::device* dev,
                             bool is_watch,
                             bool use_dummy = false)
{
  std::stringstream ss{};
  static uint64_t watch_mode_offset = 0;

  try {
    // Load config once (could be cached in a real implementation)
    std::string config_path{};
    config_path = xrt_core::device_query<xrt_core::query::firmware_log_config>(dev);
    firmware_log_config config(config_path);

    // Create and setup buffer for firmware log data
    std::vector<char> buffer;
    xrt_core::query::firmware_debug_buffer log_buffer{};
    smi_watch_mode::setup_debug_buffer(buffer, log_buffer, watch_mode_offset, is_watch);

    if (use_dummy) 
    {
      generate_dummy_firmware_log_data(log_buffer);
    } 
    else 
    {
    // Get buffer from driver
      xrt_core::device_query<xrt_core::query::firmware_log_data>(dev, log_buffer);
    }
    
    watch_mode_offset = log_buffer.abs_offset;
    
    if (!log_buffer.data) {
      ss << "No firmware log data available\n";
      return ss.str();
    }

    const auto& structures = config.get_structures();
    auto it = structures.find("ipu_log_message_header");

    const auto* data_ptr = static_cast<const uint8_t*>(log_buffer.data);
    size_t buf_size = log_buffer.size;
    size_t offset = 0;

    std::vector<Table2D::HeaderData> table_headers;
    
    // Create meaningful column headers based on ipu_log_message_header structure
    // Skip reserved fields and use descriptive names
    for (const auto& field : it->second.fields) {
      // Skip reserved fields
      if (field.name.find("reserved") != std::string::npos) {
        continue;
      }
      
      // Map field names to more descriptive column headers
      static const std::unordered_map<std::string, std::string> field_name_map = {
        {"timestamp", "Timestamp"},
        {"format", "Format"},
        {"level", "Log Level"},
        {"appn", "Application Number"},
        {"argc", "Argument Count"},
        {"line", "Line Number"},
        {"module", "Module ID"}
      };
      
      auto it_map = field_name_map.find(field.name);
      std::string header_name = (it_map != field_name_map.end()) ? it_map->second : field.name;
      
      table_headers.push_back({header_name, Table2D::Justification::left});
    }
    table_headers.push_back({"Message", Table2D::Justification::left});
    Table2D log_table(table_headers);

    size_t header_size = config.get_header_size();

    while (offset + header_size <= buf_size) {
      auto entry_data = parse_log_entry(data_ptr, offset, buf_size, config);
      log_table.addEntry(entry_data);
      
      size_t msg_size = entry_data.back().size() + 1; // +1 for null terminator
      size_t aligned_msg_size = ((msg_size + 3) / 4) * 4; // 4-byte alignment
      offset += header_size + aligned_msg_size;
    }
    ss << "  Firmware Log Information:\n";
    ss << log_table.toString("    ");
  }
  catch (const std::exception& e) {
    ss << "Error parsing firmware log data: " << e.what() << "\n";
    ss << "Generating raw firmware log data:\n\n";
    ss << generate_raw_logs(dev, is_watch, watch_mode_offset);
  }

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
  // Check for dummy option
  bool use_dummy = std::find(elements_filter.begin(), elements_filter.end(), "dummy") != elements_filter.end();
  
  // Check for watch mode
  if (smi_watch_mode::parse_watch_mode_options(elements_filter)) {
    auto report_generator = [use_dummy](const xrt_core::device* dev) -> std::string {
      return xrt_core::tools::xrt_smi::generate_firmware_log_report(dev, true, use_dummy);
    };
    smi_watch_mode::run_watch_mode(device, output, report_generator, "Firmware Log");
    return;
  }

  // Non-watch mode
  output << "Firmware Log Report\n";
  output << "===================\n\n";
  output << xrt_core::tools::xrt_smi::generate_firmware_log_report(device, false, use_dummy);
  output << std::endl;
}


