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

using boost::property_tree::ptree;

ptree 
option::
to_ptree() const 
{
  boost::property_tree::ptree pt;
  pt.put("name", name);
  pt.put("description", description);
  pt.put("type", type);
  pt.put("alias", alias);
  pt.put("default_value", default_value);
  pt.put("value_type", value_type);
  if (!description_array.empty()) {
    boost::property_tree::ptree description_array_ptree;
    for (const auto& desc : description_array) {
      boost::property_tree::ptree desc_node;
      desc_node.put("name", desc.name);
      desc_node.put("description", desc.description);
      desc_node.put("type", desc.type);
      description_array_ptree.push_back(std::make_pair("", desc_node));
    }
    pt.add_child("description_array", description_array_ptree);
  }
  return pt;
}

smi_base::
smi_base()
{

  examine_report_desc = {
    {"host", "Host information", "common"}
  };
}

std::vector<basic_option> 
smi_base::
construct_option_description(const tuple_vector& vec) const
{
  std::vector<basic_option> option_descriptions;
  for (const auto& [name, description, type] : vec) {
    option_descriptions.push_back({name, description, type});
  }
  return option_descriptions;
}

ptree 
smi_base::
construct_validate_subcommand() const 
{
  ptree subcommand;
  subcommand.put("name", "validate");
  subcommand.put("description", "Validates the given device by executing the platform's validate executable.");
  subcommand.put("type", "common");

  std::vector<option> options = {
    {"device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"},
    {"format", "f", "Report output format. Valid values are:\n"
                    "\tJSON        - Latest JSON schema\n"
                    "\tJSON-2020.2 - JSON 2020.2 schema", "common", "JSON", "string"},
    {"output", "o", "Direct the output to the given file", "common", "", "string"},
    {"help", "h", "Help to use this sub-command", "common", "", "none"},
    {"run", "r", "Run a subset of the test suite. Valid options are:\n",  "common", "",  "array", construct_option_description(validate_test_desc)},
    {"path", "p", "Path to the directory containing validate xclbins", "hidden", "", "string"},
    {"param", "", "Extended parameter for a given test. Format: <test-name>:<key>:<value>", "hidden", "", "string"},
    {"pmode", "", "Specify which power mode to run the benchmarks in. Note: Some tests might be unavailable for some modes", "hidden", "", "string"}
  };

  ptree options_ptree;
  for (const auto& option : options) {
    options_ptree.push_back(std::make_pair("", option.to_ptree()));
  }

  subcommand.add_child("options", options_ptree);
  return subcommand;
}

ptree 
smi_base::
construct_examine_subcommand() const 
{
  ptree subcommand;
  subcommand.put("name", "examine");
  subcommand.put("type", "common");
  subcommand.put("description", "This command will 'examine' the state of the system/device and will generate a report of interest in a text or JSON format.");

  std::vector<option> options = {
    {"device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"},
    {"format", "f", "Report output format. Valid values are:\n"
                    "\tJSON        - Latest JSON schema\n"
                    "\tJSON-2020.2 - JSON 2020.2 schema", "common", "", "string"},
    {"output", "o", "Direct the output to the given file", "common", "", "string"},
    {"help", "h", "Help to use this sub-command", "common", "", "none"},
    {"report", "r", "The type of report to be produced. Reports currently available are:\n", "common", "", "array", construct_option_description(examine_report_desc)},
    {"element", "e", "Filters individual elements(s) from the report. Format: '/<key>/<key>/...'", "hidden", "", "array"}
  };

  ptree options_ptree;
  for (const auto& option : options) {
    options_ptree.push_back(std::make_pair("", option.to_ptree()));
  }

  subcommand.add_child("options", options_ptree);
  return subcommand;
}

ptree 
smi_base::
construct_configure_subcommand() const 
{
  ptree subcommand;
  subcommand.put("name", "configure");
  subcommand.put("type", "common");
  subcommand.put("description", "Device and host configuration");

  std::vector<option> options = {
    {"device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"},
    {"help", "h", "Help to use this sub-command", "common", "", "none"},
    {"daemon", "", "Update the device daemon configuration", "hidden", "", "none"},
    {"purge", "", "Remove the daemon configuration file", "hidden", "", "string"},
    {"host", "", "IP or hostname for device peer", "hidden", "", "string"},
    {"security", "", "Update the security level for the device", "hidden", "", "string"},
    {"clk_throttle", "", "Enable/disable the device clock throttling", "hidden", "", "string"},
    {"ct_threshold_power_override", "", "Update the power threshold in watts", "hidden", "", "string"},
    {"ct_threshold_temp_override", "", "Update the temperature threshold in celsius", "hidden", "", "string"},
    {"ct_reset", "", "Reset all throttling options", "hidden", "", "string"},
    {"showx", "", "Display the device configuration settings", "hidden", "", "string"}
  };

  ptree options_ptree;
  for (const auto& option : options) {
    options_ptree.push_back(std::make_pair("", option.to_ptree()));
  }

  subcommand.add_child("options", options_ptree);
  return subcommand;
}

std::string 
smi_base::
get_smi_config() const 
{
  ptree config;
  ptree subcommands;

  subcommands.push_back(std::make_pair("", construct_validate_subcommand()));
  subcommands.push_back(std::make_pair("", construct_examine_subcommand()));
  subcommands.push_back(std::make_pair("", construct_configure_subcommand()));

  config.add_child("subcommands", subcommands);

  std::ostringstream oss;
  boost::property_tree::write_json(oss, config, true); // Pretty print with true
  return oss.str();
}

std::string
get_smi_config()
{
  xrt_core::smi::smi_base instance;

  return instance.get_smi_config();
}
} // namespace xrt_core::smi
