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

void
ReportFirmwareLog::
getPropertyTreeInternal(const xrt_core::device* dev, bpt& pt) const
{
  getPropertyTree20202(dev, pt);
}

void
ReportFirmwareLog::
getPropertyTree20202(const xrt_core::device* dev, bpt& pt) const
{}

std::vector<std::string> 
ReportFirmwareLog::parse_log_entry(const uint8_t* data_ptr, size_t offset, size_t buf_size, size_t header_size, const std::vector<xrt_core::tools::xrt_smi::firmware_log_config::field_info>& fields) {
  uint32_t argc = 0;
  std::vector<std::string> entry_data;

  size_t bit_offset = 0; // Start at bit 0
  for (const auto& field : fields) {
    size_t bit_width = field.width;
    size_t byte_offset = offset + (bit_offset / 8);
    size_t end_bit = bit_offset + bit_width;
    size_t end_byte = offset + ((end_bit + 7) / 8);
    if (end_byte <= offset + header_size) {
      uint64_t raw = 0;
      size_t bytes_to_read = end_byte - byte_offset;
      std::memcpy(&raw, data_ptr + byte_offset, bytes_to_read);
      size_t lsb = bit_offset % 8;
      uint64_t mask = (bit_width == 64) ? ~0ULL : ((1ULL << bit_width) - 1);
      uint64_t value = (raw >> lsb) & mask;
      if (field.name == "argc")
        argc = static_cast<uint32_t>(value);
      if (field.format.find("x") != std::string::npos)
        entry_data.push_back((bit_width == 64) ? boost::str(boost::format("0x%016X") % value) : boost::str(boost::format("0x%08X") % value));
      else
        entry_data.push_back(std::to_string(value));
    } else {
      entry_data.push_back("-");
    }
    bit_offset += bit_width; // Move to the next field
  }

  size_t msg_offset = offset + header_size;
  size_t msg_size = argc * sizeof(uint32_t);
  std::string msg_str;
  if (msg_size > 0 && msg_offset + msg_size <= buf_size) {
    std::ostringstream oss;
    for (size_t i = 0; i < argc; ++i) {
      uint32_t msg_val = 0;
      std::memcpy(&msg_val, data_ptr + msg_offset + i * 4, 4);
      oss << boost::str(boost::format("0x%08X ") % msg_val);
    }
    msg_str = oss.str();
  }
  entry_data.push_back(msg_str);

  return entry_data;
}

static std::string
generate_firmware_log_report(const xrt_core::device* dev,
                             const std::vector<std::string>& elements_filter)
{
  std::stringstream ss;

  // Load config once (could be cached in a real implementation)
  std::string config_path;
  try {
    config_path = xrt_core::device_query<xrt_core::query::firmware_log_config>(dev);
  } catch (const std::exception& e) {
    ss << "Error retrieving firmware log config path: " << e.what() << "\n";
    return ss.str();
  }

  xrt_core::tools::xrt_smi::firmware_log_config config(config_path);

  try {
    // Get raw buffer from device
    xrt_core::query::firmware_debug_buffer log_buffer = xrt_core::device_query<xrt_core::query::firmware_log_data>(dev);
    if (!log_buffer.data || log_buffer.size < 16) {
      ss << "No firmware log data available\n";
      return ss.str();
    }

    // Decode header from first 16 bytes
    const auto& structures = config.get_structures();
    auto it = structures.find("ipu_log_message_header");

    const uint8_t* data_ptr = static_cast<const uint8_t*>(log_buffer.data);
    size_t buf_size = log_buffer.size;
    size_t offset = 0;

    std::vector<Table2D::HeaderData> table_headers;
    for (const auto& field : it->second.fields)
      table_headers.push_back({field.name, Table2D::Justification::left});
    table_headers.push_back({"message", Table2D::Justification::left});
    Table2D log_table(table_headers);

    size_t header_size = config.get_header_size();

    while (offset + header_size <= buf_size) {
      auto entry_data = ReportFirmwareLog::parse_log_entry(data_ptr, offset, buf_size, header_size, it->second.fields);
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
      return generate_firmware_log_report(dev, filters);
    };
    smi_watch_mode::run_watch_mode(device, elements_filter, output,
                                   report_generator, "Firmware Log");
    return;
  }

  // Non-watch mode
  output << "Firmware Log Report\n";
  output << "===================\n\n";
  output << generate_firmware_log_report(device, elements_filter);
  output << std::endl;
}


