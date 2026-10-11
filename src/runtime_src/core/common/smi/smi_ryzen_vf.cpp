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
  // VF cannot run preemption-overhead (PF/classic only).
  validate_test_desc = {
    {"all", "All applicable validate tests will be executed (default)", "common"},
    {"df-bw", "Run bandwidth test on data fabric", "hidden"},
    {"shim-dma-bw", "Run 2xRead/1xWrite bandwidth test for SHIM DMA", "hidden"},
    {"latency", "Run end-to-end latency test", "common"},
    {"throughput", "Run end-to-end throughput test", "common"},
    {"gemm", "Measure the TOPS value of GEMM INT8operations", "common"},
    {"sanity", "Run a small model and validate sanity of the device", "hidden"}
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
