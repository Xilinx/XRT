// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.

#define XRT_CORE_COMMON_SOURCE

#include "core/common/smi/smi_ryzen_vf.h"

using namespace xrt_core::smi;

namespace xrt_core::smi::ryzen {

config_gen_npu3_vf::
config_gen_npu3_vf()
  : config_gen_npu3()
{
  // preemption-overhead enables force-preemption on the PF and runs the workload on the VF.
  validate_test_desc = {
    {"all", "All applicable validate tests will be executed (default)", "common"},
    {"df-bw", "Run bandwidth test on data fabric", "hidden"},
    {"shim-dma-bw", "Run 2xRead/1xWrite bandwidth test for SHIM DMA", "hidden"},
    {"latency", "Run end-to-end latency test", "common"},
    {"throughput", "Run end-to-end throughput test", "common"},
    {"gemm", "Measure the TOPS value of GEMM INT8operations", "common"},
    {"sanity", "Run a small model and validate sanity of the device", "hidden"},
    {"preemption-overhead", "Measure preemption overhead at noop and memtile levels", "hidden"}
  };

  examine_report_desc = {
    {"aie-partitions", "AIE partition information", "common"},
    {"all", "All known reports are produced", "common"},
    {"host", "Host information", "common"},
    {"platform", "Platforms flashed on the device", "common"},
    {"telemetry", "Telemetry data for the device", "common"},
    {"clocks", "Clock frequency information", "hidden"},
    {"debug", "Debug configuration settings for the device", "hidden"}
  };
}

subcommand
config_gen_npu3_vf::create_examine_subcommand()
{
  // firmware-log / event-trace capture and status are PF-only (same as configure).
  std::map<std::string, std::shared_ptr<option>> examine_suboptions;
  examine_suboptions.emplace("device", std::make_shared<option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  examine_suboptions.emplace("json", std::make_shared<option>("json", "", "JSON ABI version for file output. Valid values are:\n"
                                "\tdefault - Latest JSON schema (default)", "common", "default", "string"));
  examine_suboptions.emplace("output", std::make_shared<option>("output", "o", "Direct the output to the given file", "common", "", "string"));
  examine_suboptions.emplace("help", std::make_shared<option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  examine_suboptions.emplace("watch", std::make_shared<option>("watch", "", "Refresh interval in seconds between examine updates. Exit with Ctrl+C.", "hidden", "0", "string"));
  examine_suboptions.emplace("report", std::make_shared<listable_description_option>("report", "r", "The type of report to be produced. Reports currently available are:\n", "common", "", "array", get_examine_report_desc()));
  examine_suboptions.emplace("context-health", std::make_shared<option>("context-health", "", "Show status|watch context health data", "hidden", "", "string", true));

  return {"examine", "This command will 'examine' the state of the system/device and will generate a report of interest in a text or JSON format.", "common", std::move(examine_suboptions)};
}

subcommand
config_gen_npu3_vf::create_configure_subcommand()
{
  // VF cannot configure firmware-log, event-trace, or auto-coredump (PF only).
  std::map<std::string, std::shared_ptr<option>> configure_suboptions;
  configure_suboptions.emplace("device", std::make_shared<option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  configure_suboptions.emplace("help", std::make_shared<option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  configure_suboptions.emplace("pmode", std::make_shared<option>("pmode", "", "Modes: default, powersaver, balanced, performance, turbo", "common", "", "string", true));

  return {"configure", "Device and host configuration", "common", std::move(configure_suboptions)};
}

} // namespace xrt_core::smi::ryzen
