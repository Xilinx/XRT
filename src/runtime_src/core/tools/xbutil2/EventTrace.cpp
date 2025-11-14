// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/json/nlohmann/json.hpp"
#include "EventTrace.h"
#include "tools/common/XBUtilities.h"

#include <algorithm>
#include <boost/format.hpp>
#include <sstream>

namespace xrt_core::tools::xrt_smi{

event_trace_config::
event_trace_config(nlohmann::json json_config)
  : m_config(std::move(json_config)),
    m_event_bits(parse_event_bits()),
    m_payload_bits(parse_payload_bits()),
    m_file_major(parse_major_version()),
    m_file_minor(parse_minor_version()),
    m_code_tables(parse_code_table()),
    m_category_map(parse_categories()),
    m_arg_templates(parse_arg_sets()),
    m_event_map(parse_events())
{}

std::optional<event_trace_config>
event_trace_config::
load_config(const xrt_core::device* device)
{
  if (!device) {
    throw std::runtime_error("Invalid device");
  }

  auto archive = XBUtilities::open_archive(device);
  auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive.get(), {"trace_events.json"});
  auto& config_data = artifacts_repo["trace_events.json"];
  std::string config_content(config_data.begin(), config_data.end());
  
  auto json_config = nlohmann::json::parse(config_content);
  return event_trace_config(json_config);
}

uint32_t event_trace_config::
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

uint32_t event_trace_config::
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

uint16_t event_trace_config::
parse_major_version() 
{
  if (m_config.contains("version") && m_config["version"].contains("major")) {
    return m_config["version"]["major"].get<uint16_t>();
  }
  return 0;
}

uint16_t event_trace_config::
parse_minor_version() 
{
  if (m_config.contains("version") && m_config["version"].contains("minor")) {
    return m_config["version"]["minor"].get<uint16_t>();
  }
  return 0;
}

std::map<std::string, std::map<uint32_t, std::string>>
event_trace_config::
parse_code_table()
{
  std::map<std::string, std::map<uint32_t, std::string>> code_tables;
  if (!m_config.contains("lookups")) {
    return code_tables;
  }
  for (const auto& [lookup_name, lookup_entries] : m_config["lookups"].items()) {
    std::map<uint32_t, std::string> lookup_map;
    for (const auto& [key, value] : lookup_entries.items()) {
      lookup_map[std::stoul(key)] = value.get<std::string>();
    }
    code_tables[lookup_name] = lookup_map;
  }
  return code_tables;
}

std::map<std::string, event_trace_config::category_info>
event_trace_config::
parse_categories()
{
  std::map<std::string, category_info> category_map;
  if (!m_config.contains("categories")) {
    throw std::runtime_error("Missing required 'categories' section in JSON");
  }
  for (const auto& category : m_config["categories"]) {
    if (!category.contains("name")) {
      throw std::runtime_error("Category missing required 'name' field");
    }
    std::string name = category["name"].get<std::string>();
    category_info cat_info = create_category_info(category);
    category_map[name] = cat_info;
  }
  return category_map;
}

event_trace_config::category_info
event_trace_config::
create_category_info(const nlohmann::json& category) 
{
  category_info cat_info;
  cat_info.name = category["name"].get<std::string>();
  cat_info.description = category.contains("description") ? 
                         category["description"].get<std::string>() : "";
  if (category.contains("id")) {
    uint32_t id = category["id"].get<uint32_t>();
    cat_info.id = id;
  }
  return cat_info;
}

std::map<std::string, std::vector<event_trace_config::event_arg>>
event_trace_config::
parse_arg_sets()
{
  std::map<std::string, std::vector<event_arg>> arg_templates;
  if (!m_config.contains("arg_sets")) {
    return arg_templates;
  }
  for (auto it = m_config["arg_sets"].begin(); it != m_config["arg_sets"].end(); ++it) {
    std::string arg_name = it.key();
    std::vector<event_arg> args = parse_argument_list(it.value(), arg_name);
    arg_templates[arg_name] = args;
  }
  return arg_templates;
}

std::vector<event_trace_config::event_arg>
event_trace_config::
parse_argument_list(const nlohmann::json& arg_list, 
                    const std::string& arg_set_name)
{
  std::vector<event_arg> args;
  uint32_t start_position = 0;
  for (const auto& arg_data : arg_list) {
    event_arg arg = create_event_arg(arg_data, start_position, arg_set_name);
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

event_trace_config::event_arg
event_trace_config::
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
  event_arg arg;
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

std::map<uint16_t, event_trace_config::event_info>
event_trace_config::
parse_events()
{
  std::map<uint16_t, event_info> event_map;
  for (const auto& it : m_config["events"].items()) {
    const nlohmann::json& event_data = it.value();
    event_info event = create_event_info(event_data);
    event.id = static_cast<uint16_t>(std::stoul(it.key()));
    event_map[event.id] = event;
  }
  return event_map;
}

event_trace_config::event_info
event_trace_config::
create_event_info(const nlohmann::json& event_data)
{
  event_info event;
  event.name = event_data["name"].get<std::string>();
  event.description = event_data.contains("description") ? 
                      event_data["description"].get<std::string>() : "";
  event.type = "null";
  parse_event_categories(event_data, event);
  parse_event_arguments(event_data, event);
  return event;
}

void
event_trace_config::
parse_event_categories(const nlohmann::json& event_data, 
                       event_info& event)
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
event_trace_config::
parse_event_arguments(const nlohmann::json& event_data, 
                      event_info& event)
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

event_trace_config::decoded_event_t
event_trace_config::
decode_event(const event_data_t& event_data) const 
{
  decoded_event_t decoded;
  decoded.timestamp = event_data.timestamp;
  decoded.event_id = event_data.event_id;
  decoded.raw_payload = event_data.payload;
  auto event_it = m_event_map.find(event_data.event_id);
  if (event_it != m_event_map.end()) {
    const event_info& event = event_it->second;
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
event_trace_config::event_data_t
event_trace_config::
parse_buffer(const uint8_t* buffer_ptr) const
{
  const uint8_t* current_ptr = buffer_ptr;
  
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
event_trace_config::
extract_arg_value(uint64_t payload, const event_arg& arg) const 
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
event_trace_config::
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

event_trace_parser::
event_trace_parser(const event_trace_config& config) 
  : m_config(config) {}

std::string 
event_trace_parser::
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
event_trace_parser::
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
event_trace_parser::
format_categories(const std::vector<std::string>& categories) const
{
  if (categories.empty()) {
    return "";
  }
  
  std::stringstream ss{};
  for (size_t i = 0; i < categories.size(); ++i) {
    if (i > 0) ss << "|";
    ss << categories[i];
  }
  return ss.str();
}

std::string
event_trace_parser::
format_arguments(const std::map<std::string, std::string>& args) const
{
  if (args.empty()) {
    return "";
  }
  
  std::stringstream ss{};
  bool first = true;
  for (const auto& arg_pair : args) {
    if (!first) ss << ", ";
    ss << arg_pair.first << "=" << arg_pair.second;
    first = false;
  }
  return ss.str();
}

std::string
event_trace_parser::
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
