// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "EventTraceBase.h"
#include "EventTraceStrix.h"
#include "EventTraceNpu3.h"
#include "tools/common/XBUtilities.h"
#include "core/common/smi.h"
#include "core/common/query_requests.h"
#include <stdexcept>
#include <sstream>
#include <memory>

namespace xrt_core::tools::xrt_smi{

event_trace_config::
event_trace_config(nlohmann::json json_config)
  : m_config(std::move(json_config)),
    m_file_major(parse_major_version()),
    m_file_minor(parse_minor_version()),
    m_code_tables(parse_code_table()),
    m_category_map(parse_categories())
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

// Helper function to determine if hardware type is STRx
static bool
is_strix_hardware(xrt_core::smi::smi_hardware_config::hardware_type hw_type)
{
  switch (hw_type) {
    case xrt_core::smi::smi_hardware_config::hardware_type::stxA0:
    case xrt_core::smi::smi_hardware_config::hardware_type::stxB0:
    case xrt_core::smi::smi_hardware_config::hardware_type::stxH:
    case xrt_core::smi::smi_hardware_config::hardware_type::krk1:
    case xrt_core::smi::smi_hardware_config::hardware_type::phx:
      return true;
    case xrt_core::smi::smi_hardware_config::hardware_type::npu3_f1:
    case xrt_core::smi::smi_hardware_config::hardware_type::npu3_f2:
    case xrt_core::smi::smi_hardware_config::hardware_type::npu3_f3:
    case xrt_core::smi::smi_hardware_config::hardware_type::npu3_B01:
    case xrt_core::smi::smi_hardware_config::hardware_type::npu3_B02:
    case xrt_core::smi::smi_hardware_config::hardware_type::npu3_B03:
      return false;
    default:
      throw std::runtime_error("Unsupported hardware type for event trace");
  }
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

  if (is_strix_hardware(hardware_type))
    return std::make_unique<config_strix>(json_config);
  else
    return std::make_unique<config_npu3>(json_config);
}

std::unique_ptr<event_trace_parser>
event_trace_parser::
create_from_config(const std::unique_ptr<event_trace_config>& config, const xrt_core::device* device)
{
  // Detect device type using same logic as create_from_device
  const auto& pcie_id = xrt_core::device_query<xrt_core::query::pcie_id>(device);
  xrt_core::smi::smi_hardware_config smi_hrdw;
  auto hardware_type = smi_hrdw.get_hardware_type(pcie_id);
  
  if (is_strix_hardware(hardware_type)) {
    return std::make_unique<parser_strix>(static_cast<const config_strix&>(*config));
  } else {
    return std::make_unique<parser_npu3>(static_cast<const config_npu3&>(*config));
  }
}

} // namespace xrt_core::tools::xrt_smi
