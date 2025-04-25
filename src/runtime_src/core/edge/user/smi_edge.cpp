// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "smi_edge.h"

namespace shim_edge::smi {

xrt_core::smi::subcommand
create_validate_subcommand()
{
  std::vector<xrt_core::smi::basic_option> validate_test_desc = {
    {"all", "All applicable validate tests will be executed (default)", "common"},
    {"aux-connection", "Check if auxiliary power is connected", "common"},
    {"dma", "Run dma test", "common"},
    {"hostmem-bw", "Run 'bandwidth kernel' when host memory is enabled", "common"},
    {"m2m", "Run M2M test", "common"},
    {"mem-bw", "Run 'bandwidth kernel' and check the throughput", "common"},
    {"p2p", "Run P2P test", "common"},
    {"pcie-link", "Check if PCIE link is active", "common"},
    {"quick", "Only the first 4 tests will be executed", "common"},
    {"sc-version","Check if SC firmware is up-to-date", "common"},
    {"verify", "Run 'Hello World' kernel test", "common"}
  };

   std::map<std::string, std::shared_ptr<xrt_core::smi::option>> validate_suboptions;
  validate_suboptions.emplace("device", std::make_shared<xrt_core::smi::option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  validate_suboptions.emplace("format", std::make_shared<xrt_core::smi::option>("format", "f", "Report output format. Valid values are:\n"
                                "\tJSON        - Latest JSON schema\n"
                                "\tJSON-2020.2 - JSON 2020.2 schema", "common", "JSON", "string"));
  validate_suboptions.emplace("output", std::make_shared<xrt_core::smi::option>("output", "o", "Direct the output to the given file", "common", "", "string"));
  validate_suboptions.emplace("help", std::make_shared<xrt_core::smi::option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  validate_suboptions.emplace("run", std::make_shared<xrt_core::smi::listable_description_option>("run", "r", "Run a subset of the test suite. Valid options are:\n",
                              "common", "",  "array", validate_test_desc));
  validate_suboptions.emplace("path", std::make_shared<xrt_core::smi::option>("path", "p", "Path to the directory containing validate xclbins", "hidden", "", "string"));
  validate_suboptions.emplace("param", std::make_shared<xrt_core::smi::option>("param", "", "Extended parameter for a given test. Format: <test-name>:<key>:<value>", "hidden", "", "string"));
  validate_suboptions.emplace("pmode", std::make_shared<xrt_core::smi::option>("pmode", "", "Specify which power mode to run the benchmarks in. Note: Some tests might be unavailable for some modes", "hidden", "", "string")); 

  return {"validate", "Validates the given device by executing the platform's validate executable", "common", std::move(validate_suboptions)};
}

xrt_core::smi::subcommand
create_examine_subcommand()
{
  std::vector<xrt_core::smi::basic_option> examine_report_desc = {
    {"aie", "AIE metadata in xclbin", "common"},
    {"aiemem", "AIE memory tile information", "common"},
    {"aieshim", "AIE shim tile status", "common"},
    {"debug-ip-status", "Status of Debug IPs present in xclbin loaded on device", "common"},
    {"dynamic-regions", "Information about the xclbin and the compute units", "common"},
    {"electrical", "Electrical and power sensors present on the device", "common"},
    {"error", "Asyncronus Error present on the device", "common"},
    {"firewall", "Firewall status", "common"},
    {"host", "Host information", "common"},
    {"mailbox", "Mailbox metrics of the device", "common"},
    {"mechanical", "Mechanical sensors on and surrounding the device", "common"},
    {"memory", "Memory information present on the device", "common"},
    {"pcie-info", "Pcie information of the device", "common"},
    {"platform", "Platforms flashed on the device", "common"},
    {"qspi-status", "QSPI write protection status", "common"},
    {"thermal", "Thermal sensors present on the device", "common"}
  };

  std::map<std::string, std::shared_ptr<xrt_core::smi::option>> examine_suboptions; 
  examine_suboptions.emplace("device", std::make_shared<xrt_core::smi::option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  examine_suboptions.emplace("format", std::make_shared<xrt_core::smi::option>("format", "f", "Report output format. Valid values are:\n"
                                "\tJSON        - Latest JSON schema\n"
                                "\tJSON-2020.2 - JSON 2020.2 schema", "common", "JSON", "string"));
  examine_suboptions.emplace("output", std::make_shared<xrt_core::smi::option>("output", "o", "Direct the output to the given file", "common", "", "string"));
  examine_suboptions.emplace("help", std::make_shared<xrt_core::smi::option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  examine_suboptions.emplace("report", std::make_shared<xrt_core::smi::listable_description_option>("report", "r", "The type of report to be produced. Reports currently available are:\n", "common", "", "array", examine_report_desc));
  examine_suboptions.emplace("element", std::make_shared<xrt_core::smi::option>("element", "e", "Filters individual elements(s) from the report. Format: '/<key>/<key>/...'", "hidden", "", "array"));

  return {"examine", "This command will 'examine' the state of the system/device and will generate a report of interest in a text or JSON format.", "common", std::move(examine_suboptions)};
} // namespace shim_pcie::smi

xrt_core::smi::subcommand
create_configure_subcommand()
{
  std::map<std::string, std::shared_ptr<xrt_core::smi::option>> configure_suboptions;
  configure_suboptions.emplace("device", std::make_shared<xrt_core::smi::option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  configure_suboptions.emplace("help", std::make_shared<xrt_core::smi::option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  configure_suboptions.emplace("p2p", std::make_shared<xrt_core::smi::option>("p2p", "", "Controls P2P functionality\n", "common", "", "string", true));
  configure_suboptions.emplace("host-mem", std::make_shared<xrt_core::smi::option>("host-mem", "", "Controls host-mem functionality\n", "common", "", "string", true));

  return {"configure", "Device and host configuration", "common", std::move(configure_suboptions)};
}

std::string
get_smi_config()
{
  // Get the singleton instance
  auto smi_instance = xrt_core::smi::instance();

  // Add subcommands
  smi_instance->add_subcommand("validate", create_validate_subcommand());
  smi_instance->add_subcommand("examine", create_examine_subcommand());
  smi_instance->add_subcommand("configure", create_configure_subcommand());

  return smi_instance->build_smi_config();
}

} // namespace shim_edge::smi
