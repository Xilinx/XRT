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
constexpr uint32_t EVENT_BITS_DEFAULT = 16;
constexpr uint32_t PAYLOAD_BITS_DEFAULT = 48;
}

event_trace_config::
event_trace_config(const std::string& json_file_path)
  : event_bits(EVENT_BITS_DEFAULT),
    payload_bits(PAYLOAD_BITS_DEFAULT),
    file_major(0),
    file_minor(0),
    code_tables{},
    category_map{},
    arg_templates{},
    event_map{},
    config_valid(false)
{
  load_from_json(json_file_path);
}

bool 
event_trace_config::
load_from_json(const std::string& json_file_path) 
{
  try {
    nlohmann::json config = load_json_file(json_file_path);
    parse_version(config);
    parse_data_format(config);
    parse_lookups(config);
    parse_categories(config);
    parse_arg_sets(config);
    parse_events(config);
    config_valid = true;
    return true;
  } 
  catch (const std::exception& e) {
    throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
  }
}

nlohmann::json 
event_trace_config::load_json_file(const std::string& json_file_path) 
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

void 
event_trace_config::parse_data_format(const nlohmann::json& config) 
{
  if (!config.contains("data_format")) {
    throw std::runtime_error("Missing required 'data_format' section in JSON");
  }
  const auto& data_format = config["data_format"];
  if (!data_format.contains("event_bits")) {
    throw std::runtime_error("Missing 'event_bits' in data_format section");
  }
  if (!data_format.contains("payload_bits")) {
    throw std::runtime_error("Missing 'payload_bits' in data_format section");
  }
  event_bits = data_format["event_bits"].get<uint32_t>();
  payload_bits = data_format["payload_bits"].get<uint32_t>();
  if (event_bits == 0 || payload_bits == 0) {
    throw std::runtime_error("Event bits and payload bits must be greater than 0");
  }
}

void
event_trace_config::
parse_version(const nlohmann::json& config) 
{
  if (config.contains("version")) {
    const auto& version = config["version"];
    if (version.contains("major")) {
      file_major = version["major"].get<uint16_t>();
    }
    if (version.contains("minor")) {
      file_minor = version["minor"].get<uint16_t>();
    }
  }
}

void
event_trace_config::
parse_lookups(const nlohmann::json& config) 
{
  if (!config.contains("lookups")) {
    return;
  }
  for (const auto& [lookup_name, lookup_entries] : config["lookups"].items()) {
    std::map<uint32_t, std::string> lookup_map;
    for (const auto& [key, value] : lookup_entries.items()) {
      lookup_map[std::stoul(key)] = value.get<std::string>();
    }
    code_tables[lookup_name] = lookup_map;
  }
}

void
event_trace_config::
parse_categories(const nlohmann::json& config) 
{
  if (!config.contains("categories")) {
    throw std::runtime_error("Missing required 'categories' section in JSON");
  }
  std::set<std::string> name_check;
  std::set<uint32_t> forced_id_categories;
  for (const auto& category : config["categories"]) {
    if (!category.contains("name")) {
      throw std::runtime_error("Category missing required 'name' field");
    }
    std::string name = category["name"].get<std::string>();
    if (name_check.count(name)) {
      throw std::runtime_error("Duplicate category name: " + name);
    }
    name_check.insert(name);
    category_info cat_info = create_category_info(category, forced_id_categories);
    category_map[name] = cat_info;
  }
  assign_category_ids(forced_id_categories);
}

event_trace_config::category_info
event_trace_config::
create_category_info(const nlohmann::json& category, std::set<uint32_t>& forced_id_categories) 
{
  category_info cat_info;
  cat_info.name = category["name"].get<std::string>();
  cat_info.description = category.contains("description") ? category["description"].get<std::string>() : "";
  cat_info.forced_id = false;
  if (category.contains("id")) {
    uint32_t id = category["id"].get<uint32_t>();
    if (forced_id_categories.count(id)) {
      throw std::runtime_error("Duplicate category ID " + std::to_string(id) + " for category " + cat_info.name);
    }
    forced_id_categories.insert(id);
    cat_info.id = id;
    cat_info.forced_id = true;
  }
  return cat_info;
}

void
event_trace_config::
assign_category_ids(const std::set<uint32_t>& forced_id_categories) 
{
  uint32_t next_id = 0;
  for (auto& cat_pair : category_map) {
    if (!cat_pair.second.forced_id) {
      while (forced_id_categories.count(next_id)) {
        next_id++;
      }
      cat_pair.second.id = next_id++;
    }
  }
}

void
event_trace_config::
parse_arg_sets(const nlohmann::json& config) 
{
  if (!config.contains("arg_sets")) {
    return;
  }
  for (auto it = config["arg_sets"].begin(); it != config["arg_sets"].end(); ++it) {
    std::string arg_name = it.key();
    std::vector<event_arg> args = parse_argument_list(it.value(), arg_name);
    arg_templates[arg_name] = args;
  }
}

std::vector<event_trace_config::event_arg>
event_trace_config::
parse_argument_list(const nlohmann::json& arg_list, const std::string& arg_set_name) 
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
event_trace_config::
create_event_arg(const nlohmann::json& arg_data
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

void
event_trace_config::
parse_events(const nlohmann::json& config) 
{
  std::set<std::string> name_check;
  std::map<uint16_t, event_info> events_with_forced_id;
  std::vector<event_info> events_without_id;
  for (auto it = config["events"].begin(); it != config["events"].end(); ++it) {
    const nlohmann::json& event_data = it.value();
    event_info event = create_event_info(event_data, name_check);
    if (event.forced_id) {
      if (events_with_forced_id.count(event.id)) {
        throw std::runtime_error("Duplicate event ID " + std::to_string(event.id) + " for event " + event.name);
      }
      events_with_forced_id[event.id] = event;
    } else {
      events_without_id.push_back(event);
    }
  }
  assign_event_ids(events_with_forced_id, events_without_id);
  process_event_pairs(events_with_forced_id);
  event_map = events_with_forced_id;
}

event_trace_config::event_info
event_trace_config::
create_event_info(const nlohmann::json& event_data, std::set<std::string>& name_check) 
{
  event_info event;
  event.name = event_data["name"].get<std::string>();
  if (name_check.count(event.name)) {
    throw std::runtime_error("Duplicate event name: " + event.name);
  }
  name_check.insert(event.name);
  event.description = event_data.contains("description") ? event_data["description"].get<std::string>() : "";
  event.type = "null";
  event.pair_id = -1;
  parse_event_categories(event_data, event);
  parse_event_arguments(event_data, event);
  if (event_data.contains("id")) {
    event.id = static_cast<uint16_t>(event_data["id"].get<uint32_t>());
    event.forced_id = true;
  } else {
    event.forced_id = false;
  }
  return event;
}

void
event_trace_config::
parse_event_categories(const nlohmann::json& event_data, event_info& event) 
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
event_trace_config::
parse_event_arguments(const nlohmann::json& event_data, event_info& event) 
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

void
event_trace_config::
assign_event_ids(std::map<uint16_t, event_info>& events_with_forced_id,
                std::vector<event_info>& events_without_id) 
{
  uint16_t next_id = 0;
  for (auto& event : events_without_id) {
    while (events_with_forced_id.count(next_id)) {
      next_id++;
    }
    event.id = next_id;
    events_with_forced_id[next_id] = event;
    next_id++;
  }
}

void
event_trace_config::
process_event_pairs(std::map<uint16_t, event_info>& events_map) 
{
  std::map<std::string, std::map<std::string, uint16_t>> pairs;
  std::regex start_re(R"(^(.*)_START$)");
  std::regex done_re(R"(^(.*)_DONE$)");
  for (auto& event_pair : events_map) {
    event_info& event = event_pair.second;
    std::smatch match;
    if (std::regex_match(event.name, match, start_re)) {
      std::string stub = match[1].str();
      pairs[stub]["start"] = event.id;
      event.type = "start";
    } else if (std::regex_match(event.name, match, done_re)) {
      std::string stub = match[1].str();
      pairs[stub]["done"] = event.id;
      event.type = "done";
    }
  }
  for (const auto& pair : pairs) {
    const auto& pair_map = pair.second;
    if (pair_map.count("start") && pair_map.count("done")) {
      uint16_t start_id = pair_map.at("start");
      uint16_t done_id = pair_map.at("done");
      events_map[start_id].pair_id = done_id;
      events_map[done_id].pair_id = start_id;
    }
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
