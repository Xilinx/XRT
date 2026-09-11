// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.

#define XRT_CORE_COMMON_SOURCE

#include "core/common/smi/smi_ryzen_npu3.h"

using namespace xrt_core::smi;

namespace xrt_core::smi::ryzen {

config_gen_npu3::
config_gen_npu3()
{
  examine_report_desc = {
    {"aie-partitions", "AIE partition information", "common"},
    {"all", "All known reports are produced", "common"},
    {"host", "Host information", "common"},
    {"platform", "Platforms flashed on the device", "common"},
    {"telemetry", "Telemetry data for the device", "common"},
    {"preemption", "Preemption telemetry data for the device", "common"},
    {"clocks", "Clock frequency information", "hidden"},
    {"debug", "Debug configuration settings for the device", "hidden"}
  };

  validate_test_desc = {
    {"all", "All applicable validate tests will be executed (default)", "common"},
    {"runlist-latency", "Run end-to-end latency test using runlist", "hidden"},
    {"runlist-throughput", "Run end-to-end throughput test using runlist", "hidden"},
    {"df-bw", "Run bandwidth test on data fabric", "hidden"},
    {"shim-dma-bw", "Run 2xRead/1xWrite bandwidth test for SHIM DMA", "hidden"},
    {"latency", "Run end-to-end latency test", "common"},
    {"throughput", "Run end-to-end throughput test", "common"},
    {"tct-one-col", "Measure average TCT processing time for one column", "hidden"},
    {"tct-all-col", "Measure average TCT processing time for all columns", "hidden"},
    {"gemm", "Measure the TOPS value of GEMM INT8operations", "common"},
    {"sanity", "Run a small model and validate sanity of the device", "hidden"},
    {"preemption-overhead", "Measure preemption overhead at noop and memtile levels", "hidden"}
  };
}

subcommand
config_gen_npu3::create_validate_subcommand()
{
  std::map<std::string, std::shared_ptr<option>> validate_suboptions;
  validate_suboptions.emplace("device", std::make_shared<option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  validate_suboptions.emplace("json", std::make_shared<option>("json", "", "JSON ABI version for file output. Valid values are:\n"
                                "\tdefault - Latest JSON schema (default)", "common", "default", "string"));
  validate_suboptions.emplace("output", std::make_shared<option>("output", "o", "Direct the output to the given file", "common", "", "string"));
  validate_suboptions.emplace("help", std::make_shared<option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  validate_suboptions.emplace("run", std::make_shared<listable_description_option>("run", "r", "Run a subset of the test suite. Valid options are:\n",
                              "common", "",  "array", get_validate_test_desc()));
  validate_suboptions.emplace("param", std::make_shared<option>("param", "", "Extended parameter for a given test. Format: <test-name>:<key>:<value>", "param", "", "string"));
  validate_suboptions.emplace("pmode", std::make_shared<option>("pmode", "", "Specify which power mode to run the benchmarks in. Note: Some tests might be unavailable for some modes", "hidden", "", "string"));
  validate_suboptions.emplace("loop", std::make_shared<option>("loop", "", "Number of iterations to run the test", "hidden", "", "string"));

  return {"validate", "Validates the given device by executing the platform's validate executable", "common", std::move(validate_suboptions)};
}

subcommand
config_gen_npu3::create_examine_subcommand()
{
  std::map<std::string, std::shared_ptr<option>> examine_suboptions;
  examine_suboptions.emplace("device", std::make_shared<option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  examine_suboptions.emplace("json", std::make_shared<option>("json", "", "JSON ABI version for file output. Valid values are:\n"
                                "\tdefault - Latest JSON schema (default)", "common", "default", "string"));
  examine_suboptions.emplace("output", std::make_shared<option>("output", "o", "Direct the output to the given file", "common", "", "string"));
  examine_suboptions.emplace("help", std::make_shared<option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  examine_suboptions.emplace("watch", std::make_shared<option>("watch", "", "Refresh interval in seconds between examine updates. Exit with Ctrl+C.", "hidden", "0", "string"));
  examine_suboptions.emplace("report", std::make_shared<listable_description_option>("report", "r", "The type of report to be produced. Reports currently available are:\n", "common", "", "array", get_examine_report_desc()));
  examine_suboptions.emplace("firmware-log", std::make_shared<option>("firmware-log", "", "Show status|watch firmware log data", "hidden", "", "string", true));
  examine_suboptions.emplace("event-trace", std::make_shared<option>("event-trace", "", "Show status|watch event trace data", "hidden", "", "string", true));
  examine_suboptions.emplace("context-health", std::make_shared<option>("context-health", "", "Show status|watch context health data", "hidden", "", "string", true));

  return {"examine", "This command will 'examine' the state of the system/device and will generate a report of interest in a text or JSON format.", "common", std::move(examine_suboptions)};
}

subcommand
config_gen_npu3::create_configure_subcommand()
{
  std::map<std::string, std::shared_ptr<option>> configure_suboptions;
  configure_suboptions.emplace("device", std::make_shared<option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  configure_suboptions.emplace("help", std::make_shared<option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  configure_suboptions.emplace("pmode", std::make_shared<option>("pmode", "", "Modes: default, powersaver, balanced, performance, turbo", "common", "", "string", true));
  configure_suboptions.emplace("force-preemption", std::make_shared<option>("force-preemption", "", "Force enable|disable and see status of preemption", "hidden", "", "string", true));
  configure_suboptions.emplace("event-trace", std::make_shared<option>("event-trace", "", "Enable|disable event tracing", "hidden", "", "string", true));
  configure_suboptions.emplace("firmware-log", std::make_shared<option>("firmware-log", "", "Enable|disable firmware logging", "hidden", "", "string", true));
  configure_suboptions.emplace("auto-coredump", std::make_shared<option>("auto-coredump", "", "Enable|disable automatic coredump on error", "hidden", "", "string", true));

  return {"configure", "Device and host configuration", "common", std::move(configure_suboptions)};
}

} // namespace xrt_core::smi::ryzen
