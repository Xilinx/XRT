// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "EventTraceNpu3.h"
#include "tools/common/XBUtilities.h"

#include <algorithm>
#include <boost/format.hpp>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace xrt_core::tools::xrt_smi{

config_npu3::
config_npu3(nlohmann::json json_config)
  : event_trace_config(json_config),
    m_arg_templates(parse_arg_sets()),
    m_event_map(parse_events())
{
}

std::optional<config_npu3>
config_npu3::
load_config(const xrt_core::device* device)
{
  auto json_config = load_json_from_device(device);
  return config_npu3(json_config);
}

std::map<std::string, std::vector<config_npu3::event_arg_npu3>>
config_npu3::
parse_arg_sets()
{
  std::map<std::string, std::vector<event_arg_npu3>> arg_templates;
  if (!m_config.contains("arg_sets")) {
    return arg_templates;
  }
  for (auto it = m_config["arg_sets"].begin(); it != m_config["arg_sets"].end(); ++it) {
    std::string arg_name = it.key();
    std::vector<event_arg_npu3> args = parse_argument_list(it.value(), arg_name);
    arg_templates[arg_name] = args;
  }
  return arg_templates;
}

std::vector<config_npu3::event_arg_npu3>
config_npu3::
parse_argument_list(const nlohmann::json& arg_list,
                    const std::string& arg_set_name)
{
  std::vector<event_arg_npu3> args;
  
  for (const auto& arg_data : arg_list) {
    event_arg_npu3 arg = create_event_arg(arg_data, arg_set_name);
    args.push_back(arg);
  }
  
  return args;
}

config_npu3::event_arg_npu3
config_npu3::
create_event_arg(const nlohmann::json& arg_data,
                 const std::string& arg_set_name)
{
  if (!arg_data.contains("name")) {
    throw std::runtime_error("Argument in arg_set '" + arg_set_name + "' missing 'name' field");
  }
  if (!arg_data.contains("type")) {
    throw std::runtime_error("Argument in arg_set '" + arg_set_name + "' missing 'type' field");
  }

  event_arg_npu3 arg;
  arg.name = arg_data["name"].get<std::string>();
  arg.type = arg_data["type"].get<std::string>();
  arg.count = arg_data.value("count", 1);
  arg.format = arg_data.value("format", "");
  arg.lookup = arg_data.value("lookup", "");
  arg.signed_field = arg_data.value("signed", false);
  arg.description = arg_data.value("description", "");
  
  return arg;
}

std::map<uint16_t, config_npu3::event_info_npu3>
config_npu3::
parse_events()
{
  std::map<uint16_t, event_info_npu3> event_map;
  if (!m_config.contains("events")) {
    return event_map;
  }
  
  for (const auto& it : m_config["events"].items()) {
    const nlohmann::json& event_data = it.value();
    event_info_npu3 event = create_event_info(event_data);
    event.id = static_cast<uint16_t>(std::stoul(it.key()));
    event_map[event.id] = event;
  }
  return event_map;
}

config_npu3::event_info_npu3
config_npu3::
create_event_info(const nlohmann::json& event_data)
{
  event_info_npu3 event;
  event.name = event_data["name"].get<std::string>();
  event.description = event_data.contains("description") ? 
                      event_data["description"].get<std::string>() : "";
  event.type = "null";

  parse_event_categories(event_data, event);
  parse_event_arguments(event_data, event);
  
  return event;
}

void
config_npu3::
parse_event_categories(const nlohmann::json& event_data,
                       event_info_npu3& event)
{
  uint32_t category_mask = 0;
  if (event_data.contains("categories")) {
    for (const auto& cat_name : event_data["categories"]) {
      std::string cat_name_str = cat_name.get<std::string>();
      event.categories.push_back(cat_name_str);
      auto cat_it = m_category_map.find(cat_name_str);
      if (cat_it == m_category_map.end()) {
        throw std::runtime_error("Event '" + event.name + "' references unknown category: " + cat_name_str);
      }
      const auto& cat_info = cat_it->second;
      category_mask |= (1U << cat_info.id);
    }
  }
  event.category_mask = category_mask;
}

void
config_npu3::
parse_event_arguments(const nlohmann::json& event_data,
                      event_info_npu3& event)
{
  event.args_name = event_data.contains("args_name") ? 
                    event_data["args_name"].get<std::string>() : "";
  if (!event.args_name.empty()) {
    auto arg_it = m_arg_templates.find(event.args_name);
    if (arg_it == m_arg_templates.end()) {
      throw std::runtime_error("Event '" + event.name + "' references unknown arg_set: " + event.args_name);
    }
    event.args = arg_it->second;
  }
}

config_npu3::event_data_t
config_npu3::
parse_buffer(const uint8_t* buffer_ptr) const
{
  const uint8_t* current_ptr = buffer_ptr;
  
  // Parse timestamp (8 bytes)
  uint64_t timestamp;
  std::memcpy(&timestamp, current_ptr, sizeof(uint64_t));
  current_ptr += sizeof(uint64_t);
  
  // Parse magic byte (should be 0xAA)
  uint8_t magic = *current_ptr++;
  if (magic != npu3_magic_byte) {
    throw std::runtime_error("Invalid NPU3 event magic byte: 0x" + 
                           (std::ostringstream() << std::hex << static_cast<int>(magic)).str());
  }
  
  // Parse category_id (2 bytes)
  uint16_t category_id;
  std::memcpy(&category_id, current_ptr, sizeof(uint16_t));
  current_ptr += sizeof(uint16_t);
  
  // Parse payload_size (1 byte)
  uint8_t payload_size = *current_ptr++;
  
  // Payload pointer
  const uint8_t* payload_ptr = current_ptr;
  
  return {timestamp, category_id, payload_ptr, payload_size};
}

config_npu3::decoded_event_t
config_npu3::
decode_event(const event_data_t& event_data) const
{
  decoded_event_t decoded;
  decoded.timestamp = event_data.timestamp;
  decoded.event_id = event_data.category_id;
  
  auto event_it = m_event_map.find(event_data.category_id);
  if (event_it != m_event_map.end()) {
    const event_info_npu3& event = event_it->second;
    decoded.name = event.name;
    decoded.description = event.description;
    decoded.categories = event.categories;
    
    // Extract arguments from struct payload
    size_t offset = 0;
    for (const auto& arg : event.args) {
      try {
        std::string value = extract_arg_value(event_data.payload_ptr, offset, arg);
        decoded.args[arg.name] = value;
      } catch (const std::exception& e) {
        decoded.args[arg.name] = "ERROR: " + std::string(e.what());
      }
    }
  } else {
    decoded.name = "UNKNOWN";
    decoded.description = "Unknown event ID: " + std::to_string(event_data.category_id);
    decoded.categories = {"UNKNOWN"};
  }
  
  return decoded;
}

std::string
config_npu3::
get_event_name(uint16_t event_id) const
{
  auto it = m_event_map.find(event_id);
  if (it != m_event_map.end()) {
    return it->second.name;
  }
  return "UNKNOWN";
}

std::optional<config_npu3::event_info_npu3>
config_npu3::
get_event_info(uint16_t event_id) const
{
  auto it = m_event_map.find(event_id);
  if (it != m_event_map.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::string
config_npu3::
extract_arg_value(const uint8_t* payload_ptr,
                  size_t& offset,
                  const event_arg_npu3& arg) const
{
  size_t type_size = get_type_size(arg.type);
  std::stringstream ss;
  
  // Handle arrays
  if (arg.count > 1) {
    ss << "[";
    for (uint32_t i = 0; i < arg.count; ++i) {
      if (i > 0) ss << ",";
      
      uint64_t value = 0;
      std::memcpy(&value, payload_ptr + offset, type_size);
      offset += type_size;
      
      // Check for lookup
      if (!arg.lookup.empty()) {
        auto lookup_it = m_code_tables.find(arg.lookup);
        if (lookup_it != m_code_tables.end()) {
          auto value_it = lookup_it->second.find(static_cast<uint32_t>(value));
          if (value_it != lookup_it->second.end()) {
            ss << value_it->second;
            continue;
          }
        }
      }
      
      ss << format_value(value, arg.format);
    }
    ss << "]";
  } else {
    // Single value
    uint64_t value = 0;
    std::memcpy(&value, payload_ptr + offset, type_size);
    offset += type_size;
    
    // Check for lookup
    if (!arg.lookup.empty()) {
      auto lookup_it = m_code_tables.find(arg.lookup);
      if (lookup_it != m_code_tables.end()) {
        auto value_it = lookup_it->second.find(static_cast<uint32_t>(value));
        if (value_it != lookup_it->second.end()) {
          return value_it->second;
        }
      }
      return format_value(value, arg.format) + " [lookup:" + arg.lookup + "]";
    }
    
    return format_value(value, arg.format);
  }
  
  return ss.str();
}

size_t
config_npu3::
get_type_size(const std::string& type) const
{
  if (type == "uint8") return 1;
  if (type == "uint16") return 2;
  if (type == "uint32") return 4;
  if (type == "uint64") return 8;
  throw std::runtime_error("Unknown type: " + type);
}

std::string
config_npu3::
format_value(uint64_t value, const std::string& format) const
{
  if (format.empty() || format == "d") {
    return std::to_string(value);
  }
  
  std::ostringstream oss;
  if (format.find('x') != std::string::npos) {
    // Extract width from format string (e.g., "08x" -> width=8)
    std::string width_str;
    for (char c : format) {
      if (std::isdigit(c)) {
        width_str += c;
      }
    }
    if (!width_str.empty()) {
      int width = std::stoi(width_str);
      oss << "0x" << std::hex << std::setw(width) << std::setfill('0') << value;
    } else {
      oss << "0x" << std::hex << value;
    }
  } else {
    oss << value;
  }
  
  return oss.str();
}

parser_npu3::
parser_npu3(const config_npu3& config)
  : m_config(config)
{
}

std::string
parser_npu3::
parse(const uint8_t* data_ptr, size_t buf_size) const
{
  std::stringstream ss;
  
  if (!data_ptr || buf_size == 0) {
    return "No event trace data available\n";
  }

  const uint8_t* current_ptr = data_ptr;
  const uint8_t* end_ptr = data_ptr + buf_size;
  
  // Parse each variable-size event
  while (current_ptr < end_ptr) {
    // Check if we have enough bytes for minimum event header
    if (current_ptr + npu3_header_bytes > end_ptr) {
      break;  // Not enough data for another event
    }
    
    try {
      // Parse NPU3 event
      auto event_data = m_config.parse_buffer(current_ptr);
      
      // Decode event
      auto decoded_event = m_config.decode_event(event_data);
      
      // Format output
      ss << format_event(decoded_event);
      
      // Advance pointer: header(12) + payload_size
      current_ptr += npu3_header_bytes + event_data.payload_size;
      
    } catch (const std::exception& e) {
      ss << "Error parsing event: " << e.what() << "\n";
      break;
    }
  }
  
  return ss.str();
}

std::string
parser_npu3::
format_event(const decoded_event_t& decoded_event) const
{
  std::stringstream ss;
  
  // Format categories
  std::string categories_str = format_categories(decoded_event.categories);
  
  // Format arguments
  std::string args_str = format_arguments(decoded_event.args);
  
  // Format as table row with consistent column widths
  std::string event_name = decoded_event.name.empty() ? "UNKNOWN" : decoded_event.name;
  std::string category_display = categories_str.empty() ? "UNKNOWN" : categories_str;
  
  ss << boost::format("%-20lu %-25s %-25s %-30s\n")
        % decoded_event.timestamp
        % event_name
        % category_display
        % args_str;
  
  return ss.str();
}

} // namespace xrt_core::tools::xrt_smi
