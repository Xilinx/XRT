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
#include <vector>

using bpt = boost::property_tree::ptree;

namespace xrt_core::tools::xrt_smi {

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

  if (ctx.field && ctx.field->format.find("x") != std::string::npos) {
    return (ctx.bit_width == bits_per_uint64) ? boost::str(boost::format("0x%0" + std::to_string(hex_width_64) + "X") % value) : boost::str(boost::format("0x%08X") % value);
  } 
  else if (ctx.field && ctx.field->name == "level") {
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
  size_t msg_size = argc * sizeof(uint32_t);
  if (msg_size > 0 && msg_offset + msg_size <= buf_size) {
    std::ostringstream oss;
    for (size_t i = 0; i < argc; ++i) {
      uint32_t msg_val = 0;
      std::memcpy(&msg_val, data_ptr + msg_offset + i * 4, 4);
      oss << boost::str(boost::format("0x%08X ") % msg_val);
    }
    return oss.str();
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
      entry_data.emplace_back(field_value);
    } else {
      entry_data.emplace_back("-");
    }
    bit_offset += bit_width;
  }

  size_t msg_offset = offset + header_size;
  entry_data.emplace_back(parse_message(data_ptr, msg_offset, argc, buf_size));
  return entry_data;
}

static std::string
generate_firmware_log_report(const xrt_core::device* dev,
                             bool is_watch)
{
  std::stringstream ss;

  // Load config once (could be cached in a real implementation)
  std::string config_path;
  try {
    config_path = xrt_core::device_query<xrt_core::query::firmware_log_config>(dev, is_watch);
  } catch (const std::exception& e) {
    ss << "Error retrieving firmware log config path: " << e.what() << "\n";
    return ss.str();
  }

  firmware_log_config config(config_path);

  try {
    // Get raw buffer from device
    xrt_core::query::firmware_debug_buffer log_buffer = xrt_core::device_query<xrt_core::query::firmware_log_data>(dev);
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
    for (const auto& field : it->second.fields)
      table_headers.push_back({field.name, Table2D::Justification::left});
    table_headers.push_back({"message", Table2D::Justification::left});
    Table2D log_table(table_headers);

    size_t header_size = config.get_header_size();

    while (offset + header_size <= buf_size) {
      auto entry_data = parse_log_entry(data_ptr, offset, buf_size, config);
      log_table.addEntry(entry_data);
      size_t msg_size = entry_data.back().size() * sizeof(uint32_t); // Approximate message size
      size_t aligned_msg_size = ((msg_size + 3) / 4) * 4; // 4-byte alignment
      offset += header_size + aligned_msg_size;
    }
    ss << "  Firmware Log Information:\n";
    ss << log_table.toString("    ");
  }
  catch (const std::exception& e) {
    ss << "Error retrieving firmware log data: " << e.what() << "\n";
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
  // Check for watch mode
  if (smi_watch_mode::parse_watch_mode_options(elements_filter)) {
    auto report_generator = [](const xrt_core::device* dev, const std::vector<std::string>& filters) -> std::string {
      return xrt_core::tools::xrt_smi::generate_firmware_log_report(dev, true);
    };
    smi_watch_mode::run_watch_mode(device, elements_filter, output,
                                   report_generator, "Firmware Log");
    return;
  }

  // Non-watch mode
  output << "Firmware Log Report\n";
  output << "===================\n\n";
  output << xrt_core::tools::xrt_smi::generate_firmware_log_report(device, false);
  output << std::endl;
}


