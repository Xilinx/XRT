// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#define XRT_CORE_COMMON_SOURCE

// Local - Include Files
#include "smi.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <string>
#include <vector>

namespace xrt_core::smi {

using ptree = boost::property_tree::ptree;

ptree 
option::
to_ptree() const 
{
  boost::property_tree::ptree pt;
  pt.put("name", m_name);
  pt.put("description", m_description);
  pt.put("type", m_type);
  pt.put("alias", m_alias);
  pt.put("default_value", m_default_value);
  pt.put("value_type", m_value_type);
  return pt;
}

ptree
listable_description_option::
to_ptree() const 
{
  boost::property_tree::ptree pt = option::to_ptree();
  boost::property_tree::ptree description_array_ptree;
  for (const auto& desc : m_description_array) {
    boost::property_tree::ptree desc_node;
    desc_node.put("name", desc.m_name);
    desc_node.put("description", desc.m_description);
    desc_node.put("type", desc.m_type);
    description_array_ptree.push_back(std::make_pair("", desc_node));
  }
  pt.add_child("description_array", description_array_ptree);
  return pt;
}

tuple_vector
listable_description_option::
get_description_array() const 
{
  tuple_vector desc_array;
  for (const auto& desc : m_description_array) {
    desc_array.emplace_back(std::make_tuple(desc.m_name, desc.m_description, desc.m_type));
  }
  return desc_array;
}

ptree 
subcommand::
construct_subcommand_json() const 
{
  boost::property_tree::ptree pt;
  pt.put("name", m_name);
  pt.put("description", m_description);
  pt.put("type", m_type);

  boost::property_tree::ptree options_ptree;
  for (const auto& opt : m_options) {
    options_ptree.push_back(std::make_pair("", opt.second->to_ptree()));
  }
  pt.add_child("options", options_ptree);
  return pt;
}

tuple_vector
subcommand::
get_option_options() const 
{
  tuple_vector option_options;
  for (const auto& [name, option] : m_options) {
    if (option->get_is_optionOption()) {
      option_options.emplace_back(std::make_tuple(name, option->m_description, option->m_type));
    }
  }
  return option_options;
}

std::string 
smi::
build_json() const 
{
  ptree config;
  ptree subcommands;

  for (const auto& [name, subcmd] : m_subcommands) {
    subcommands.push_back(std::make_pair("", subcmd.construct_subcommand_json()));
  }

  config.add_child("subcommands", subcommands);

  std::ostringstream oss;
  boost::property_tree::write_json(oss, config, true); 
  return oss.str();
}

tuple_vector
smi::
get_list(const std::string& subcommand, const std::string& suboption) const 
{
  const auto it = m_subcommands.find(subcommand);
  if (it == m_subcommands.end()) {
    throw std::runtime_error("Subcommand not found: " + subcommand);
  }

  const auto& subcmd = it->second;

  const auto it_suboption = subcmd.get_options().find(suboption);
  if (it_suboption == subcmd.get_options().end()) {
    throw std::runtime_error("Suboption not found: " + suboption);
  }

  const auto& option = it_suboption->second;
  return option->get_description_array();
}

tuple_vector
smi::
get_option_options(const std::string& subcommand) const 
{
  const auto it = m_subcommands.find(subcommand);
  if (it == m_subcommands.end()) {
    throw std::runtime_error("Subcommand not found: " + subcommand);
  }

  const auto& subcmd = it->second;

  return subcmd.get_option_options();
}

smi*
instance() 
{
  static smi instance;
  return &instance;
}

smi_hardware_config::
smi_hardware_config()
{
  // Initialize the hardware map
  hardware_map = {
    {{0x1502, 0x00}, hardware_type::phx},
    {{0x17f0, 0x00}, hardware_type::stxA0},
    {{0x17f0, 0x10}, hardware_type::stxB0},
    {{0x17f0, 0x11}, hardware_type::stxH},
    {{0x17f0, 0x20}, hardware_type::krk1},
    {{0x17f1, 0x10}, hardware_type::npu3_f1}, 
    {{0x17f2, 0x10}, hardware_type::npu3_f2}, 
    {{0x17f3, 0x10}, hardware_type::npu3_f3},
    {{0x1B0A, 0x00}, hardware_type::npu3_B01},
    {{0x1B0B, 0x00}, hardware_type::npu3_B02},
    {{0x1B0C, 0x00}, hardware_type::npu3_B03}
  };
}

smi_hardware_config::hardware_type
smi_hardware_config::
get_hardware_type(const xq::pcie_id::data& dev) const 
{
  auto it = hardware_map.find(dev);
  return (it != hardware_map.end()) 
      ? it->second 
      : hardware_type::unknown;
}

tuple_vector
get_list(const std::string& subcommand, const std::string& suboption) 
{
  return instance()->get_list(subcommand, suboption);
}

tuple_vector
get_option_options(const std::string& subcommand) 
{
  return instance()->get_option_options(subcommand);
}

} // namespace xrt_core::smi
