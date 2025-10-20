// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <fstream>
#include <cstring>
#include "FirmwareLog.h"

namespace xrt_core::tools::xrt_smi {

firmware_log_config::
firmware_log_config(nlohmann::json json_config)
  : m_config(std::move(json_config)),
    m_enums(parse_enums(m_config)),
    m_structures(parse_structures(m_config)),
    m_header_size(calculate_header_size(m_structures))
{}


std::unordered_map<std::string, firmware_log_config::enum_info>
firmware_log_config::
parse_enums(const nlohmann::json& config)
{
  std::unordered_map<std::string, enum_info> enums_map;
  if (!config.contains("enumerations")) {
    return enums_map;
  }
  const auto& enums_json = config["enumerations"];
  for (auto it = enums_json.begin(); it != enums_json.end(); ++it) {
    enum_info enum_info;
    enum_info.m_name = it.key();
    if (it.value().contains("enumerators")) {
      for (const auto& [name, value] : it.value()["enumerators"].items()) {
        enum_info.enumerator_to_value[name] = value;
        enum_info.value_to_enumerator[value] = name;
      }
    }
    enums_map[enum_info.m_name] = enum_info;
  }
  return enums_map;
}

std::unordered_map<std::string, firmware_log_config::structure_info>
firmware_log_config::
parse_structures(const nlohmann::json& config)
{
  std::unordered_map<std::string, structure_info> structs_map;
  if (!config.contains("structures")) {
    return structs_map;
  }
  const auto& structs_json = config["structures"];
  for (auto it = structs_json.begin(); it != structs_json.end(); ++it) {
    structure_info struct_info;
    struct_info.name = it.key();
    if (it.value().contains("fields")) {
      for (const auto& field : it.value()["fields"]) {
        field_info field_info;
        field_info.name = field.value("name", "");
        field_info.type = field.value("type", "");
        field_info.width = field.value("width", 0);
        field_info.format = field.value("format", "");
        field_info.enumeration = field.value("enumeration", "");
        struct_info.fields.push_back(field_info);
      }
    }
    structs_map[struct_info.name] = struct_info;
  }
  return structs_map;
}

std::string 
firmware_log_config::enum_info::
get_enumerator_name(uint32_t value) const 
{
  auto it = value_to_enumerator.find(value);
  return it != value_to_enumerator.end() ? it->second : "<unknown>";
}

uint32_t 
firmware_log_config::enum_info::
get_enumerator_value(const std::string& name) const 
{
  auto it = enumerator_to_value.find(name);
  return it != enumerator_to_value.end() ? it->second : 0;
}

const firmware_log_config::structure_info& 
firmware_log_config::
get_log_header() const 
{
  auto it = m_structures.find("ipu_log_message_header");
  if (it == m_structures.end()) {
    throw std::runtime_error("ipu_log_message_header structure not found in config");
  }
  return it->second;
}

size_t 
firmware_log_config::
calculate_header_size(const std::unordered_map<std::string, structure_info>& structures) {
  auto it = structures.find("ipu_log_message_header");
  if (it == structures.end()) {
    throw std::runtime_error("Config missing ipu_log_message_header structure");
  }
  size_t size = 0;
  for (const auto& field : it->second.fields) {
    size += field.width;
  }
  return (size + byte_alignment) / bits_per_byte; // Convert bit width to byte size
}

firmware_log_parser::
firmware_log_parser(firmware_log_config config) 
  : m_config(std::move(config)), 
    m_header(m_config.get_log_header()),
    m_header_size(static_cast<uint32_t>(m_config.get_header_size())),
    m_field_indices(create_field_indices(m_config)),
    m_columns({
      {"timestamp", "Timestamp"},
      {"level", "Log-Level"},
      {"appn", "App Number "},
      {"line", "Line Number"},
      {"module", "Module ID"}
    }),
    m_column_widths(create_column_widths(m_columns))
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

std::unordered_map<std::string, size_t> 
firmware_log_parser::
create_column_widths(const std::unordered_map<std::string, std::string>& columns)
{
  std::unordered_map<std::string, size_t> widths;
  for (const auto& [field_name, header_text] : columns) {
    widths[field_name] = header_text.length() + 4; // Add padding for alignment
  }
  return widths;
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

uint32_t 
firmware_log_parser::
calculate_entry_size(uint32_t argc, uint32_t format) const
{
  uint32_t entry_size = argc;
  if (format == 0) {
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


std::string
firmware_log_parser::
get_header_row() const
{
  std::string result;
  
  for (const auto& field : m_header.fields) {
    if (m_columns.find(field.name) != m_columns.end()) {
      const std::string& header_text = m_columns.at(field.name);
      size_t width = m_column_widths.at(field.name);
      
      result += header_text;
      // Pad with spaces to reach column width
      if (header_text.length() < width) {
        result.append(width - header_text.length(), ' ');
      }
    }
  }
  result += "Message\n";
  return result;
}

std::string
firmware_log_parser::
format_entry_row(const std::vector<std::string>& entry_data) const
{
  std::string result;
  for (const auto& field : m_header.fields) {
    if (m_columns.find(field.name) != m_columns.end()) {
      const std::string& data_text = entry_data[m_field_indices.at(field.name)];
      size_t width = m_column_widths.at(field.name);
      
      result += data_text;
      // Pad with spaces to reach column width
      if (data_text.length() < width) {
        result.append(width - data_text.length(), ' ');
      }
    }
  }
  result += entry_data.back() + '\n'; // Add message
  return result;
}

std::string
firmware_log_parser::
parse(const uint8_t* data_ptr, size_t buf_size) const
{
  std::string result;
  result += get_header_row();
  
  size_t offset = 0;
  while (offset + m_header_size <= buf_size) {
    auto entry_data = parse_entry(data_ptr, offset, buf_size);
    auto format = std::stoul(entry_data[m_field_indices.at("format")]); 
    auto argc = std::stoul(entry_data[m_field_indices.at("argc")]);
    
    result += format_entry_row(entry_data);
    
    size_t entry_size = calculate_entry_size(argc, format);
    offset += entry_size;
  }
  return result;
}

} // namespace xrt_core::tools::xrt_smi
