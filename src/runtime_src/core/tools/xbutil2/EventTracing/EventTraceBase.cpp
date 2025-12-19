// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "EventTraceBase.h"
#include "EventTraceStrix.h"
#include "EventTraceNpu3.h"

#include "core/common/query_requests.h"
#include "tools/common/XBUtilities.h"

#include <memory>
#include <stdexcept>
#include <sstream>

namespace xrt_core::tools::xrt_smi{

event_trace_config::
event_trace_config(nlohmann::json json_config)
  : m_config(std::move(json_config)),
    m_file_major(parse_major_version()),
    m_file_minor(parse_minor_version()),
    m_code_tables(parse_code_table()),
    m_category_map(parse_categories()),
    m_entry_header_size(parse_structure_size("ring_buffer_entry_header")),
    m_entry_footer_size(parse_structure_size("ring_buffer_entry_footer"))
{
}

nlohmann::json
event_trace_config::
load_json_from_device(const xrt_core::device* device)
{
  if (!device) {
    throw std::runtime_error("Invalid device");
  }

  auto archive = XBUtilities::open_archive(device);
  auto artifacts_repo = XBUtilities::extract_artifacts_from_archive(archive.get(), {"trace_events.json"});
  auto& config_data = artifacts_repo["trace_events.json"];
  std::string config_content(config_data.begin(), config_data.end());
  
  return nlohmann::json::parse(config_content);
}

uint16_t
event_trace_config::
parse_major_version()
{
  if (m_config.contains("version") && m_config["version"].contains("major")) {
    return m_config["version"]["major"].get<uint16_t>();
  }
  return 0;
}

uint16_t
event_trace_config::
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
    cat_info.id = category["id"].get<uint32_t>();
  }
  return cat_info;
}

size_t
event_trace_config::
parse_structure_size(const std::string& struct_name)
{
  if (!m_config.contains("structures") || !m_config["structures"].contains(struct_name)) {
    return 0;
  }
  const auto& structure = m_config["structures"][struct_name];
  if (structure.contains("size")) {
    return structure["size"].get<size_t>();
  }
  return 0;
}

// Parser base implementation
std::string
event_trace_parser::
format_categories(const std::vector<std::string>& categories) const
{
  if (categories.empty()) {
    return "";
  }
  
  std::stringstream ss;
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
  
  std::stringstream ss;
  bool first = true;
  for (const auto& [name, value] : args) {
    if (!first) ss << ", ";
    ss << name << "=" << value;
    first = false;
  }
  return ss.str();
}

std::unique_ptr<event_trace_config>
event_trace_config::
create_from_device(const xrt_core::device* device)
{
  auto json_config = load_json_from_device(device);

  // Determine the hardware type
  using query = xrt_core::query::pcie_id;
  auto pcie_id = xrt_core::device_query<query>(device);

  smi::smi_hardware_config smi_hrdw;
  auto hardware_type = smi_hrdw.get_hardware_type(pcie_id);

  if (XBUtilities::is_strix_hardware(hardware_type))
    return std::make_unique<config_strix>(json_config);
  else
    return std::make_unique<config_npu3>(json_config);
}

std::map<std::string, uint32_t>
event_trace_config::
get_category_map(const xrt_core::device* device) {
  std::map<std::string, uint32_t> category_map;

  // Load categories from config
  try {
    auto config = create_from_device(device);
    
    // Get categories from config and convert ID to mask
    const auto& config_categories = config->get_categories();
    for (const auto& [name, info] : config_categories) {
      uint32_t mask = (1U << info.id);
      category_map[name] = mask;
    }
  } catch (const std::exception&) {
    // Config loading failed, return empty map
  }
  return category_map;
}

std::vector<std::string>
event_trace_config::
mask_to_category_names(uint32_t mask, const xrt_core::device* device) {
  std::vector<std::string> category_names;
  
  if (mask == 0) {
    return category_names; // Empty list for no categories
  }
  
  if (mask == 0xFFFFFFFF) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)  
    category_names.emplace_back("ALL");
    return category_names;
  }
  
  auto category_map = get_category_map(device);
  
  for (const auto& [name, category_mask] : category_map) {
    if (mask & category_mask) {
      category_names.push_back(name);
    }
  }
  
  return category_names;
}

std::unique_ptr<event_trace_parser>
event_trace_parser::
create_from_config(const std::unique_ptr<event_trace_config>& config,
                   const xrt_core::device* device)
{
  // Detect device type using same logic as create_from_device
  const auto& pcie_id = xrt_core::device_query<xrt_core::query::pcie_id>(device);
  xrt_core::smi::smi_hardware_config smi_hrdw;
  auto hardware_type = smi_hrdw.get_hardware_type(pcie_id);
  
  if (XBUtilities::is_strix_hardware(hardware_type)) {
    return std::make_unique<parser_strix>(dynamic_cast<const config_strix&>(*config));
  } else {
    return std::make_unique<parser_npu3>(dynamic_cast<const config_npu3&>(*config));
  }
}

} // namespace xrt_core::tools::xrt_smi
