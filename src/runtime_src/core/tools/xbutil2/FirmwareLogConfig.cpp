// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <fstream>
#include "FirmwareLogConfig.h"

namespace xrt_core::tools::xrt_smi {

static nlohmann::json 
load_json_config(const std::string& json_file_path) {
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

firmware_log_config::
firmware_log_config(const std::string& json_file_path)
  : config(load_json_config(json_file_path)),
    enums(parse_enums(config)),
    structures(parse_structures(config)),
    header_size(calculate_header_size(structures))
{}


std::map<std::string, firmware_log_config::enum_info>
firmware_log_config::
parse_enums(const nlohmann::json& config)
{
  std::map<std::string, enum_info> enums_map;
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

std::map<std::string, firmware_log_config::structure_info>
firmware_log_config::
parse_structures(const nlohmann::json& config)
{
  std::map<std::string, structure_info> structs_map;
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
get_enumerator_name(uint32_t value) const {
  auto it = value_to_enumerator.find(value);
  return it != value_to_enumerator.end() ? it->second : "<unknown>";
}

uint32_t 
firmware_log_config::enum_info::
get_enumerator_value(const std::string& name) const {
  auto it = enumerator_to_value.find(name);
  return it != enumerator_to_value.end() ? it->second : 0;
}

size_t 
firmware_log_config::
calculate_header_size(const std::map<std::string, structure_info>& structures) {
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

} // namespace xrt_core::tools::xrt_smi
