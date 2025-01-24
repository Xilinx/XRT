// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "smi.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <variant>

namespace xrt_core::smi {

using boost::property_tree::ptree;

static const std::vector<std::tuple<std::string, std::string, std::string>> AieValidateTestDesc = {
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

static const std::vector<std::tuple<std::string, std::string, std::string>> AlveoValidateTestDesc = {
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

//In default cases, all validate tests should be shown
static const auto DefaultValidateTestDesc = [] {
    std::vector<std::tuple<std::string, std::string, std::string>> result;
    result.reserve(AieValidateTestDesc.size() + AlveoValidateTestDesc.size());
    result.insert(result.end(), AieValidateTestDesc.begin(), AieValidateTestDesc.end());
    result.insert(result.end(), AlveoValidateTestDesc.begin(), AlveoValidateTestDesc.end());
    return result;
}();

static const std::vector<std::tuple<std::string, std::string, std::string>> AieExamineReportDesc = {
    {"aie-partitions", "AIE partition information", "common"},
    {"host", "Host information", "common"},
    {"platform", "Platforms flashed on the device", "common"},
    {"telemetry", "Telemetry data for the device", "common"}
}; 

static const std::vector<std::tuple<std::string, std::string, std::string>> AlveoExamineReportDesc = {
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
static const auto DefaultExamineReportDesc = [] {
    std::vector<std::tuple<std::string, std::string, std::string>> result;
    result.reserve(AieExamineReportDesc.size() + AlveoExamineReportDesc.size());
    result.insert(result.end(), AieExamineReportDesc.begin(), AieExamineReportDesc.end());
    result.insert(result.end(), AlveoExamineReportDesc.begin(), AlveoExamineReportDesc.end());
    return result;
}();
    
  
struct BasicOption {
    std::string name;
    std::string description;
    std::string type;
};

struct Option : public BasicOption {
    std::string alias;
    std::string default_value;
    std::string value_type;
    std::vector<BasicOption> description_array;

    Option(const std::string& name, 
           const std::string& alias, 
           const std::string& description,
           const std::string& type, 
           const std::string& default_value, 
           const std::string& value_type, 
           const std::vector<BasicOption>& description_array = {})
        : BasicOption{name, description, type}, 
          alias(alias), 
          default_value(default_value), 
          value_type(value_type), 
          description_array(description_array) {}

    ptree toPtree() const {
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

ptree constructValidateSubcommand(bool isDefault = true) {
    ptree subcommand;
    subcommand.put("name", "validate");
    subcommand.put("description", "Validates the given device by executing the platform's validate executable.");
    subcommand.put("type", "common");

    auto constructRunOptionDescription = [&]() {
        std::vector<BasicOption> runOptionDescriptions;
        for (const auto& [name, description, type] : isDefault ? DefaultValidateTestDesc : AlveoValidateTestDesc) {
            runOptionDescriptions.push_back({name, description, type});
        }
        return runOptionDescriptions;
    };

    std::vector<Option> options = {
        {"device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"},
        {"format", "f", "Report output format. Valid values are:\n"
                        "\tJSON        - Latest JSON schema\n"
                        "\tJSON-2020.2 - JSON 2020.2 schema", "common", "JSON", "string"},
        {"output", "o", "Direct the output to the given file", "common", "", "string"},
        {"help", "h", "Help to use this sub-command", "common", "", "none"},
        {"run", "r", "Run a subset of the test suite. Valid options are:\n",  "common", "",  "array", constructRunOptionDescription()},
        {"path", "p", "Path to the directory containing validate xclbins", "hidden", "", "string"},
        {"param", "", "Extended parameter for a given test. Format: <test-name>:<key>:<value>", "hidden", "", "string"},
        {"pmode", "", "Specify which power mode to run the benchmarks in. Note: Some tests might be unavailable for some modes", "hidden", "", "string"}
    };

    ptree options_ptree;
    for (const auto& option : options) {
        options_ptree.push_back(std::make_pair("", option.toPtree()));
    }

    subcommand.add_child("options", options_ptree);
    return subcommand;
}

ptree constructExamineSubcommand(bool isDefault = true) {
    ptree subcommand;
    subcommand.put("name", "examine");
    subcommand.put("type", "common");
    subcommand.put("description", "This command will 'examine' the state of the system/device and will generate a report of interest in a text or JSON format.");

    auto constructReportOptionDescription = [&]() {
        std::vector<BasicOption> reportOptionDescriptions;
        for (const auto& [name, description, type] : isDefault ? DefaultExamineReportDesc : AlveoExamineReportDesc) {
            reportOptionDescriptions.push_back({name, description, type});
        }
        return reportOptionDescriptions;
    };

    std::vector<Option> options = {
        {"device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"},
        {"format", "f", "Report output format. Valid values are:\n"
                        "\tJSON        - Latest JSON schema\n"
                        "\tJSON-2020.2 - JSON 2020.2 schema", "common", "", "string"},
        {"output", "o", "Direct the output to the given file", "common", "", "string"},
        {"help", "h", "Help to use this sub-command", "common", "", "none"},
        {"report", "r", "The type of report to be produced. Reports currently available are:\n", "common", "", "array", constructReportOptionDescription()},
        {"element", "e", "Filters individual elements(s) from the report. Format: '/<key>/<key>/...'", "hidden", "", "array"}
    };

    ptree options_ptree;
    for (const auto& option : options) {
        options_ptree.push_back(std::make_pair("", option.toPtree()));
    }

    subcommand.add_child("options", options_ptree);
    return subcommand;
}

ptree constructConfigureSubcommand() {
    ptree subcommand;
    subcommand.put("name", "configure");
    subcommand.put("type", "common");
    subcommand.put("description", "Device and host configuration");

    std::vector<Option> options = {
        {"device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"},
        {"help", "h", "Help to use this sub-command", "common", "", "none"},
        {"daemon", "", "Update the device daemon configuration", "hidden", "", "none"},
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
        options_ptree.push_back(std::make_pair("", option.toPtree()));
    }

    subcommand.add_child("options", options_ptree);
    return subcommand;
}

/*
 * This function is primarily for xrt-smi help printing.
 * No execution level detail should be queried from the configuration
 * return by this API. 
*/
std::string get_smi_config() {
    ptree config;
    ptree subcommands;

    subcommands.push_back(std::make_pair("", constructValidateSubcommand(false)));
    subcommands.push_back(std::make_pair("", constructExamineSubcommand(false)));
    subcommands.push_back(std::make_pair("", constructConfigureSubcommand()));

    config.add_child("subcommands", subcommands);

    std::ostringstream oss;
    boost::property_tree::write_json(oss, config, true); 

    // Since the mode of interface between shim and XRT is decided
    // to be kept as native C++ as possible, converting to string.
    return oss.str();
}

std::string get_default_smi_config() {
    ptree config;
    ptree subcommands;

    subcommands.push_back(std::make_pair("", constructValidateSubcommand()));
    subcommands.push_back(std::make_pair("", constructExamineSubcommand()));
    subcommands.push_back(std::make_pair("", constructConfigureSubcommand()));

    config.add_child("subcommands", subcommands);

    std::ostringstream oss;
    boost::property_tree::write_json(oss, config, true); 

    // Since the mode of interface between shim and XRT is decided
    // to be kept as native C++ as possible, converting to string.
    return oss.str();
}
} // namespace xrt_core::smi
