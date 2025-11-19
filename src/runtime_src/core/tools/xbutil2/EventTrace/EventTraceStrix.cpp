// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/json/nlohmann/json.hpp"
#include "EventTraceStrix.h"
#include "tools/common/XBUtilities.h"

#include <algorithm>
#include <boost/format.hpp>
#include <sstream>

namespace xrt_core::tools::xrt_smi{

config_strix::
config_strix(nlohmann::json json_config)
  : event_trace_config(json_config),
    m_event_bits(parse_event_bits()),
    m_payload_bits(parse_payload_bits()),
    m_arg_templates(parse_arg_sets()),
    m_event_map(parse_events())
{
}

std::optional<config_strix>
config_strix::
load_config(const xrt_core::device* device)
{
  auto json_config = load_json_from_device(device);
  return config_strix(json_config);
}

uint32_t
config_strix::
parse_event_bits() 
{
  if (!m_config.contains("data_format") || !m_config["data_format"].contains("event_bits")) {
    return event_bits_default;
  }
  uint32_t event_bits_val = m_config["data_format"]["event_bits"].get<uint32_t>();
  if (event_bits_val == 0) {
    throw std::runtime_error("Event bits must be greater than 0");
  }
  return event_bits_val;
}

uint32_t
config_strix::
parse_payload_bits() 
{
  if (!m_config.contains("data_format") || !m_config["data_format"].contains("payload_bits")) {
    return payload_bits_default;
  }
  uint32_t payload_bits_val = m_config["data_format"]["payload_bits"].get<uint32_t>();
  if (payload_bits_val == 0) {
    throw std::runtime_error("Payload bits must be greater than 0");
  }
  return payload_bits_val;
}

std::map<std::string, std::vector<config_strix::event_arg_strix>>
config_strix::
parse_arg_sets()
{
  std::map<std::string, std::vector<event_arg_strix>> arg_templates;
  if (!m_config.contains("arg_sets")) {
    return arg_templates;
  }
  for (auto it = m_config["arg_sets"].begin(); it != m_config["arg_sets"].end(); ++it) {
    std::string arg_name = it.key();
    std::vector<event_arg_strix> args = parse_argument_list(it.value(), arg_name);
    arg_templates[arg_name] = args;
  }
  return arg_templates;
}

std::vector<config_strix::event_arg_strix>
config_strix::
parse_argument_list(const nlohmann::json& arg_list, 
                    const std::string& arg_set_name)
{
  std::vector<event_arg_strix> args;
  uint32_t start_position = 0;
  for (const auto& arg_data : arg_list) {
    event_arg_strix arg = create_event_arg(arg_data, start_position, arg_set_name);
    start_position += arg.width;
    if (start_position > m_payload_bits) {
      throw std::runtime_error("Argument '" 
                                + arg.name 
                                + "' in arg_set '" 
                                + arg_set_name 
                                + "' exceeds payload bits (" 
                                + std::to_string(m_payload_bits) + ")");
    }
    args.push_back(arg);
  }
  return args;
}

config_strix::event_arg_strix
config_strix::
create_event_arg(const nlohmann::json& arg_data, 
                 uint32_t start_position, 
                 const std::string& arg_set_name) 
{
  if (!arg_data.contains("name")) {
    throw std::runtime_error("Argument in arg_set '" + arg_set_name + "' missing 'name' field");
  }
  if (!arg_data.contains("width")) {
    throw std::runtime_error("Argument in arg_set '" + arg_set_name + "' missing 'width' field");
  }
  event_arg_strix arg;
  arg.name = arg_data["name"].get<std::string>();
  arg.width = arg_data["width"].get<uint32_t>();
  arg.start = start_position;
  arg.format = arg_data.contains("format") ? arg_data["format"].get<std::string>() : "";
  arg.description = arg_data.contains("description") ? 
                    arg_data["description"].get<std::string>() : "";

  arg.lookup = arg_data.contains("lookup") ? arg_data["lookup"].get<std::string>() : "";
  arg.signed_field = arg_data.contains("signed") ? arg_data["signed"].get<bool>() : false;
  if (arg.width == 0) {
    throw std::runtime_error("Argument '" + arg.name + "' width cannot be zero");
  }
  return arg;
}

std::map<uint16_t, config_strix::event_info_strix>
config_strix::
parse_events()
{
  std::map<uint16_t, event_info_strix> event_map;
  for (const auto& it : m_config["events"].items()) {
    const nlohmann::json& event_data = it.value();
    event_info_strix event = create_event_info(event_data);
    event.id = static_cast<uint16_t>(std::stoul(it.key()));
    event_map[event.id] = event;
  }
  return event_map;
}

config_strix::event_info_strix
config_strix::
create_event_info(const nlohmann::json& event_data)
{
  event_info_strix event;
  event.name = event_data["name"].get<std::string>();
  event.description = event_data.contains("description") ? 
                      event_data["description"].get<std::string>() : "";
  event.type = "null";
  parse_event_categories(event_data, event);
  parse_event_arguments(event_data, event);
  return event;
}

void
config_strix::
parse_event_categories(const nlohmann::json& event_data, 
                       event_info_strix& event)
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
config_strix::
parse_event_arguments(const nlohmann::json& event_data, 
                      event_info_strix& event)
{
  event.args_name = event_data.contains("args_name") ? event_data["args_name"].get<std::string>() : "";
  if (!event.args_name.empty()) {
    auto arg_it = m_arg_templates.find(event.args_name);
    if (arg_it == m_arg_templates.end()) {
      throw std::runtime_error("Event '" + event.name + "' references unknown arg_set: " + event.args_name);
    }
    event.args = arg_it->second;
  }
}

config_strix::decoded_event_t
config_strix::
decode_event(const event_data_t& event_data) const 
{
  decoded_event_t decoded;
  decoded.timestamp = event_data.timestamp;
  decoded.event_id = event_data.event_id;
  decoded.raw_payload = event_data.payload;
  auto event_it = m_event_map.find(event_data.event_id);
  if (event_it != m_event_map.end()) {
    const event_info_strix& event = event_it->second;
    decoded.name = event.name;
    decoded.description = event.description;
    decoded.categories = event.categories;
    for (const auto& arg : event.args) {
      try {
        std::string value = extract_arg_value(event_data.payload, arg);
        decoded.args[arg.name] = value;
      } 
      catch (const std::exception& e) {
        decoded.args[arg.name] = "ERROR: " + std::string(e.what());
      }
    }
  } else {
    decoded.name = "UNKNOWN";
    decoded.description = "Unknown event ID: " + std::to_string(event_data.event_id);
    decoded.categories = {"UNKNOWN"};
  }
  return decoded;
}
/*
Example:

Event ID: 0x0014
Payload: 0x000000094ee0000f
Payload bits: 48
What firmware writes as combined_value:
combined = (0x0014 << 48) | 0x000000094ee0000f
combined = 0x0014000000000000 | 0x000000094ee0000f  
combined = 0x00140094ee0000f

Thus using the same mechanism for extraction
*/
config_strix::event_data_t
config_strix::
parse_buffer(const uint8_t* data_ptr) const
{
  const uint8_t* current_ptr = data_ptr;
  
  // Parse timestamp (always 8 bytes)
  uint64_t timestamp = *reinterpret_cast<const uint64_t*>(current_ptr);
  current_ptr += timestamp_bytes_default;
  
  // Parse combined event_id and payload from single 64-bit value
  uint64_t combined_value = *reinterpret_cast<const uint64_t*>(current_ptr);
  
  // Extract event_id from upper bits
  auto event_id = static_cast<uint16_t>(combined_value >> m_payload_bits);
  
  // Extract payload from lower bits
  uint64_t payload_mask = (1ULL << m_payload_bits) - 1;
  uint64_t payload = combined_value & payload_mask;
  
  return {timestamp, event_id, payload};
}

std::string
config_strix::
extract_arg_value(uint64_t payload, 
                  const event_arg_strix& arg) const 
{
  uint64_t mask = (1ULL << arg.width) - 1;
  uint64_t value = (payload >> arg.start) & mask;
  if (arg.signed_field && (value & (1ULL << (arg.width - 1)))) {
    value |= (~mask);
  }
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

std::string
config_strix::
format_value(uint64_t value, const std::string& format) const 
{
  if (format.empty() || format == "d") {
    return std::to_string(value);
  }
  std::ostringstream oss;
  if (format.find('x') != std::string::npos) {
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

parser_strix::
parser_strix(const config_strix& config) 
  : m_config(config) {}

std::string
parser_strix::
parse(const uint8_t* data_ptr, size_t buf_size) const
{
  std::stringstream ss{};
  
  if (!data_ptr || buf_size == 0) {
    return "No event trace data available\n";
  }

  // Calculate total event size from config
  size_t total_event_size = m_config.get_event_size();
  size_t event_count = buf_size / total_event_size;
  
  // Parse each event dynamically based on config sizes
  const uint8_t* current_ptr = data_ptr;
  for (size_t i = 0; i < event_count; ++i) {
    // Parse event from buffer using runtime config
    auto event_data = m_config.parse_buffer(current_ptr);
    current_ptr += total_event_size;
    
    ss << format_event(event_data);
  }
  return ss.str();
}

std::string
parser_strix::
format_event(const event_data_t& event_data) const
{
  std::stringstream ss{};
  
  auto decoded_event = m_config.decode_event(event_data);

  // Format categories for table
  std::string categories_str = format_categories(decoded_event.categories);

  // Format arguments for table
  std::string args_str = format_arguments(decoded_event.args);

  // Format as table row with consistent column widths
  std::string event_name = decoded_event.name.empty() ? "UNKNOWN" : decoded_event.name;
  std::string category_display = categories_str.empty() ? "UNKNOWN" : categories_str;
  
  ss << boost::format("%-20lu %-25s %-25s %-30s\n")//NOLINT (cppcoreguidelines-avoid-magic-numbers) 
        % event_data.timestamp               // Use parsed timestamp value
        % event_name                         // Use parsed name or UNKNOWN
        % category_display                   // Use parsed categories or UNKNOWN
        % args_str;
  
  return ss.str();
}

std::string
parser_strix::
format_summary(size_t event_count, size_t buf_size) const
{
  std::stringstream ss{};
  ss << "Event Trace Summary\n";
  ss << "===================\n";
  ss << "Total Events: " << event_count << "\n";
  ss << "Buffer Size: " << buf_size << " bytes\n\n";
  return ss.str();
}

} // namespace xrt_core::tools::xrt_smi
