// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "smi.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <vector>
#include <tuple>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace xrt_core::smi {

using boost::property_tree::ptree;

static const std::vector<std::tuple<std::string, std::string, std::string>> aie_validate_test_desc = {
  {"aie-reconfig-overhead", "Run end-to-end array reconfiguration overhead through shim DMA", "hidden"},
  {"all", "All applicable validate tests will be executed (default)", "common"},
  {"cmd-chain-latency", "Run end-to-end latency test using command chaining", "common"},
  {"cmd-chain-throughput", "Run end-to-end throughput test using command chaining", "common"},
  {"df-bw", "Run bandwidth test on data fabric", "common"},
  {"gemm", "Measure the TOPS value of GEMM operations", "common"},
  {"latency", "Run end-to-end latency test", "common"},
  {"quick", "Run a subset of four tests: \n1. latency \n2. throughput \n3. cmd-chain-latency \n4. cmd-chain-throughput", "common"},
  {"spatial-sharing-overhead", "Run Spatial Sharing Overhead Test", "hidden"},
  {"tct-all-col", "Measure average TCT processing time for all columns", "common"},
  {"tct-one-col", "Measure average TCT processing time for one column", "common"},
  {"temporal-sharing-overhead", "Run Temporal Sharing Overhead Test", "hidden"},
  {"throughput", "Run end-to-end throughput test", "common"}
};

static const std::vector<std::tuple<std::string, std::string, std::string>> alveo_validate_test_desc = {
  {"aux-connection", "Check if auxiliary power is connected", "common"},
  {"dma", "Run dma test", "common"},
  {"thostmem-bw", "Run 'bandwidth kernel' when host memory is enabled", "common"},
  {"m2m", "Run M2M test", "common"},
  {"mem-bw", "Run 'bandwidth kernel' and check the throughput", "common"},
  {"p2p", "Run P2P test", "common"},
  {"pcie-link", "Check if PCIE link is active", "common"},
  {"sc-version","Check if SC firmware is up-to-date", "common"},
  {"verify", "Run 'Hello World' kernel test", "common"}
};

// In default cases, all validate tests should be shown
static const std::vector<std::tuple<std::string, std::string, std::string>> default_validate_test_desc = [] {
  std::vector<std::tuple<std::string, std::string, std::string>> result;
  result.reserve(aie_validate_test_desc.size() + alveo_validate_test_desc.size());
  result.insert(result.end(), aie_validate_test_desc.begin(), aie_validate_test_desc.end());
  result.insert(result.end(), alveo_validate_test_desc.begin(), alveo_validate_test_desc.end());
  return result;
}();

static const std::vector<std::tuple<std::string, std::string, std::string>> aie_examine_report_desc = {
  {"aie-partitions", "AIE partition information", "common"},
  {"host", "Host information", "common"},
  {"platform", "Platforms flashed on the device", "common"},
  {"telemetry", "Telemetry data for the device", "common"}
};

static const std::vector<std::tuple<std::string, std::string, std::string>> alveo_examine_report_desc = {
  {"aie", "AIE metadata in xclbin", "common"},
  {"aiemem", "AIE memory tile information", "common"},
  {"aieshim", "AIE shim tile status", "common"},
  {"debug-ip-status", "Status of Debug IPs present in xclbin loaded on device", "common"},
  {"dynamic-regions", "Information about the xclbin and the compute units", "common"},
  {"electrical", "Electrical and power sensors present on the device", "common"},
  {"error", "Asyncronus Error present on the device", "common"},
  {"firewall", "Firewall status", "common"},
  {"mailbox", "Mailbox metrics of the device", "common"},
  {"mechanical", "Mechanical sensors on and surrounding the device", "common"},
  {"memory", "Memory information present on the device", "common"},
  {"pcie-info", "Pcie information of the device", "common"},
  {"qspi-status", "QSPI write protection status", "common"},
  {"thermal", "Thermal sensors present on the device", "common"}
};

// In default cases, all reports should be printed
static const std::vector<std::tuple<std::string, std::string, std::string>> default_examine_report_desc = [] {
  std::vector<std::tuple<std::string, std::string, std::string>> result;
  result.reserve(aie_examine_report_desc.size() + alveo_examine_report_desc.size());
  result.insert(result.end(), aie_examine_report_desc.begin(), aie_examine_report_desc.end());
  result.insert(result.end(), alveo_examine_report_desc.begin(), alveo_examine_report_desc.end());
  return result;
}();

struct basic_option {
  std::string name;
  std::string description;
  std::string type;
};

struct option : public basic_option {
  std::string alias;
  std::string default_value;
  std::string value_type;
  std::vector<basic_option> description_array;

  option(const std::string& name, 
         const std::string& alias, 
         const std::string& description,
         const std::string& type, 
         const std::string& default_value, 
         const std::string& value_type, 
         const std::vector<basic_option>& description_array = {})
      : basic_option{name, description, type}, 
        alias(alias), 
        default_value(default_value), 
        value_type(value_type), 
        description_array(description_array) {}

  ptree to_ptree() const {
    ptree pt;
    pt.put("name", name);
    pt.put("description", description);
    pt.put("type", type);
    pt.put("alias", alias);
    pt.put("default_value", default_value);
    pt.put("value_type", value_type);
    if (!description_array.empty()) {
      ptree description_array_ptree;
      for (const auto& desc : description_array) {
        ptree desc_node;
        desc_node.put("name", desc.name);
        desc_node.put("description", desc.description);
        desc_node.put("type", desc.type);
        description_array_ptree.push_back(std::make_pair("", desc_node));
      }
      pt.add_child("description_array", description_array_ptree);
    }
    return pt;
  }
};

ptree construct_validate_subcommand(bool is_default = true) {
  ptree subcommand;
  subcommand.put("name", "validate");
  subcommand.put("description", "Validates the given device by executing the platform's validate executable.");
  subcommand.put("type", "common");

  auto construct_run_option_description = [&]() {
    std::vector<basic_option> run_option_descriptions;
    for (const auto& [name, description, type] : is_default ? default_validate_test_desc : alveo_validate_test_desc) {
      run_option_descriptions.push_back({name, description, type});
    }
    return run_option_descriptions;
  };

  std::vector<option> options = {
    {"device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"},
    {"format", "f", "Report output format. Valid values are:\n"
                    "\tJSON        - Latest JSON schema\n"
                    "\tJSON-2020.2 - JSON 2020.2 schema", "common", "JSON", "string"},
    {"output", "o", "Direct the output to the given file", "common", "", "string"},
    {"help", "h", "Help to use this sub-command", "common", "", "none"},
    {"run", "r", "Run a subset of the test suite. Valid options are:\n",  "common", "",  "array", construct_run_option_description()},
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

ptree construct_examine_subcommand(bool is_default = true) {
  ptree subcommand;
  subcommand.put("name", "examine");
  subcommand.put("type", "common");
  subcommand.put("description", "This command will 'examine' the state of the system/device and will generate a report of interest in a text or JSON format.");

  auto construct_report_option_description = [&]() {
    std::vector<basic_option> report_option_descriptions;
    for (const auto& [name, description, type] : is_default ? default_examine_report_desc : alveo_examine_report_desc) {
      report_option_descriptions.push_back({name, description, type});
    }
    return report_option_descriptions;
  };

  std::vector<option> options = {
    {"device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"},
    {"format", "f", "Report output format. Valid values are:\n"
                    "\tJSON        - Latest JSON schema\n"
                    "\tJSON-2020.2 - JSON 2020.2 schema", "common", "", "string"},
    {"output", "o", "Direct the output to the given file", "common", "", "string"},
    {"help", "h", "Help to use this sub-command", "common", "", "none"},
    {"report", "r", "The type of report to be produced. Reports currently available are:\n", "common", "", "array", construct_report_option_description()},
    {"element", "e", "Filters individual elements(s) from the report. Format: '/<key>/<key>/...'", "hidden", "", "array"}
  };

  ptree options_ptree;
  for (const auto& option : options) {
    options_ptree.push_back(std::make_pair("", option.to_ptree()));
  }

  subcommand.add_child("options", options_ptree);
  return subcommand;
}

ptree construct_configure_subcommand() {
  ptree subcommand;
  subcommand.put("name", "configure");
  subcommand.put("type", "common");
  subcommand.put("description", "Device and host configuration");

  std::vector<option> options = {
    {"device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"},
    {"help", "h", "Help to use this sub-command", "common", "", "none"},
    {"daemon", "", "Update the device daemon configuration", "common", "", "none"},
    {"purge", "", "Remove the daemon configuration file", "hidden", "", "string"},
    {"host", "", "IP or hostname for device peer", "common", "", "string"},
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

std::string get_smi_config() {
  ptree config;
  ptree subcommands;

  subcommands.push_back(std::make_pair("", construct_validate_subcommand(false)));
  subcommands.push_back(std::make_pair("", construct_examine_subcommand(false)));
  subcommands.push_back(std::make_pair("", construct_configure_subcommand()));

  config.add_child("subcommands", subcommands);

  std::ostringstream oss;
  boost::property_tree::write_json(oss, config, true); // Pretty print with true
  return oss.str();
}

std::string get_default_smi_config() {
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

} // namespace xrt_core::smi
