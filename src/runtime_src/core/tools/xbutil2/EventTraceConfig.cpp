// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "EventTraceConfig.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>

#include "core/common/json/nlohmann/json.hpp"


namespace {

constexpr uint32_t event_bits_default = 16;
constexpr uint32_t payload_bits_default = 48;

static nlohmann::json 
load_json_file(const std::string& json_file_path) 
{
  if (json_file_path.empty()) {
    throw std::runtime_error("JSON file path cannot be empty");
  }
  std::ifstream file(json_file_path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open JSON file: " + json_file_path);
  }
  nlohmann::json j;
  file >> j;
  return j;
}

} // End of unnamed namespace
namespace xrt_core::tools::xrt_smi{

event_trace_config::
event_trace_config(const std::string& json_file_path)
  : config(load_json_file(json_file_path)),
    event_bits(parse_event_bits(config)),
    payload_bits(parse_payload_bits(config)),
    file_major(parse_major_version(config)),
    file_minor(parse_minor_version(config)),
    code_tables(parse_code_table(config)),
    category_map(parse_categories(config)),
    arg_templates(parse_arg_sets(config, payload_bits)),
    event_map(parse_events(config, category_map, arg_templates))
{}

uint32_t event_trace_config::parse_event_bits(const nlohmann::json& config) {
  if (!config.contains("data_format") || !config["data_format"].contains("event_bits")) {
    return event_bits_default;
  }
  uint32_t event_bits_val = config["data_format"]["event_bits"].get<uint32_t>();
  if (event_bits_val == 0) {
    throw std::runtime_error("Event bits must be greater than 0");
  }
  return event_bits_val;
}

uint32_t event_trace_config::parse_payload_bits(const nlohmann::json& config) {
  if (!config.contains("data_format") || !config["data_format"].contains("payload_bits")) {
    return payload_bits_default;
  }
  uint32_t payload_bits_val = config["data_format"]["payload_bits"].get<uint32_t>();
  if (payload_bits_val == 0) {
    throw std::runtime_error("Payload bits must be greater than 0");
  }
  return payload_bits_val;
}

uint16_t event_trace_config::parse_major_version(const nlohmann::json& config) {
  if (config.contains("version") && config["version"].contains("major")) {
    return config["version"]["major"].get<uint16_t>();
  }
  return 0;
}

uint16_t event_trace_config::parse_minor_version(const nlohmann::json& config) {
  if (config.contains("version") && config["version"].contains("minor")) {
    return config["version"]["minor"].get<uint16_t>();
  }
  return 0;
}

std::map<std::string, std::map<uint32_t, std::string>>
event_trace_config::parse_code_table(const nlohmann::json& config)
{
  std::map<std::string, std::map<uint32_t, std::string>> code_tables;
  if (!config.contains("lookups")) {
    return code_tables;
  }
  for (const auto& [lookup_name, lookup_entries] : config["lookups"].items()) {
    std::map<uint32_t, std::string> lookup_map;
    for (const auto& [key, value] : lookup_entries.items()) {
      lookup_map[std::stoul(key)] = value.get<std::string>();
    }
    code_tables[lookup_name] = lookup_map;
  }
  return code_tables;
}

std::map<std::string, event_trace_config::category_info>
event_trace_config::parse_categories(const nlohmann::json& config)
{
  std::map<std::string, category_info> category_map;
  if (!config.contains("categories")) {
    throw std::runtime_error("Missing required 'categories' section in JSON");
  }
  for (const auto& category : config["categories"]) {
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
event_trace_config::create_category_info(const nlohmann::json& category) 
{
  category_info cat_info;
  cat_info.name = category["name"].get<std::string>();
  cat_info.description = category.contains("description") ? category["description"].get<std::string>() : "";
  if (category.contains("id")) {
    uint32_t id = category["id"].get<uint32_t>();
    cat_info.id = id;
  }
  return cat_info;
}

std::map<std::string, std::vector<event_trace_config::event_arg>>
event_trace_config::parse_arg_sets(const nlohmann::json& config, uint32_t payload_bits)
{
  std::map<std::string, std::vector<event_arg>> arg_templates;
  if (!config.contains("arg_sets")) {
    return arg_templates;
  }
  for (auto it = config["arg_sets"].begin(); it != config["arg_sets"].end(); ++it) {
    std::string arg_name = it.key();
    std::vector<event_arg> args = parse_argument_list(it.value(), arg_name, payload_bits);
    arg_templates[arg_name] = args;
  }
  return arg_templates;
}

std::vector<event_trace_config::event_arg>
event_trace_config::parse_argument_list(const nlohmann::json& arg_list, const std::string& arg_set_name, uint32_t payload_bits)
{
  std::vector<event_arg> args;
  uint32_t start_position = 0;
  for (const auto& arg_data : arg_list) {
    event_arg arg = create_event_arg(arg_data, start_position, arg_set_name);
    start_position += arg.width;
    if (start_position > payload_bits) {
      throw std::runtime_error("Argument '" + arg.name + "' in arg_set '" + arg_set_name + "' exceeds payload bits (" + std::to_string(payload_bits) + ")");
    }
    args.push_back(arg);
  }
  return args;
}

event_trace_config::event_arg
event_trace_config::create_event_arg(const nlohmann::json& arg_data
                 , uint32_t start_position
                 , const std::string& arg_set_name) 
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
  arg.description = arg_data.contains("description") ? arg_data["description"].get<std::string>() : "";
  arg.lookup = arg_data.contains("lookup") ? arg_data["lookup"].get<std::string>() : "";
  arg.signed_field = arg_data.contains("signed") ? arg_data["signed"].get<bool>() : false;
  if (arg.width == 0) {
    throw std::runtime_error("Argument '" + arg.name + "' width cannot be zero");
  }
  return arg;
}

std::map<uint16_t, event_trace_config::event_info>
event_trace_config::parse_events(const nlohmann::json& config, 
                                 const std::map<std::string, category_info>& category_map, 
                                 const std::map<std::string, std::vector<event_arg>>& arg_templates)
{
  std::map<uint16_t, event_info> event_map;
  for (const auto& it : config["events"].items()) {
    const nlohmann::json& event_data = it.value();
    event_info event = create_event_info(event_data, category_map, arg_templates);
    event.id = static_cast<uint16_t>(std::stoul(it.key()));
    event_map[event.id] = event;
  }
  return event_map;
}

event_trace_config::event_info
event_trace_config::create_event_info(const nlohmann::json& event_data, 
                                      const std::map<std::string, category_info>& category_map, 
                                      const std::map<std::string, std::vector<event_arg>>& arg_templates)
{
  event_info event;
  event.name = event_data["name"].get<std::string>();
  event.description = event_data.contains("description") ? event_data["description"].get<std::string>() : "";
  event.type = "null";
  parse_event_categories(event_data, event, category_map);
  parse_event_arguments(event_data, event, arg_templates);
  return event;
}

void
event_trace_config::parse_event_categories(const nlohmann::json& event_data, 
                                           event_info& event, 
                                           const std::map<std::string, category_info>& category_map)
{
  uint32_t category_mask = 0;
  if (event_data.contains("categories")) {
    for (const auto& cat_name : event_data["categories"]) {
      std::string cat_name_str = cat_name.get<std::string>();
      event.categories.push_back(cat_name_str);
      auto cat_it = category_map.find(cat_name_str);
      if (cat_it == category_map.end()) {
        throw std::runtime_error("Event '" + event.name + "' references unknown category: " + cat_name_str);
      }
      const auto& cat_info = cat_it->second;
      category_mask |= (1U << cat_info.id);
    }
  }
  event.category_mask = category_mask;
}

void
event_trace_config::parse_event_arguments(const nlohmann::json& event_data, 
                                          event_info& event, const std::map<std::string, 
                                          std::vector<event_arg>>& arg_templates)
{
  event.args_name = event_data.contains("args_name") ? event_data["args_name"].get<std::string>() : "";
  if (!event.args_name.empty()) {
    auto arg_it = arg_templates.find(event.args_name);
    if (arg_it == arg_templates.end()) {
      throw std::runtime_error("Event '" + event.name + "' references unknown arg_set: " + event.args_name);
    }
    event.args = arg_it->second;
  }
}

event_trace_config::parsed_event
event_trace_config::
parse_event(const event_record& record) const 
{
  parsed_event parsed;
  parsed.timestamp = record.timestamp;
  parsed.event_id = record.event_id;
  parsed.raw_payload = record.payload;
  auto event_it = event_map.find(record.event_id);
  if (event_it != event_map.end()) {
    const event_info& event = event_it->second;
    parsed.name = event.name;
    parsed.description = event.description;
    parsed.categories = event.categories;
    for (const auto& arg : event.args) {
      try {
        std::string value = extract_arg_value(record.payload, arg);
        parsed.args[arg.name] = value;
      } 
      catch (const std::exception& e) {
        parsed.args[arg.name] = "ERROR: " + std::string(e.what());
      }
    }
  } else {
    parsed.name = "UNKNOWN";
    parsed.description = "Unknown event ID: " + std::to_string(record.event_id);
    parsed.categories = {"UNKNOWN"};
  }
  return parsed;
}

std::string
event_trace_config::
get_event_name(uint16_t event_id) const 
{
  auto it = event_map.find(event_id);
  return it != event_map.end() ? it->second.name : "UNKNOWN";
}

std::vector<std::string>
event_trace_config::
get_event_categories(uint16_t event_id) const 
{
  auto it = event_map.find(event_id);
  return it != event_map.end() ? it->second.categories : std::vector<std::string>{"UNKNOWN"};
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
    auto lookup_it = code_tables.find(arg.lookup);
    if (lookup_it != code_tables.end()) {
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

} // namespace xrt_core::tools::xrt_smi
