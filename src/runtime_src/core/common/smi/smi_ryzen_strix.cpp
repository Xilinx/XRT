// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.

#define XRT_CORE_COMMON_SOURCE

#include "core/common/smi/smi_ryzen_strix.h"

namespace xrt_core::smi::ryzen {

config_gen_strix::
config_gen_strix()
{
  examine_report_desc = {
    {"aie-partitions", "AIE partition information", "common"},
    {"all", "All known reports are produced", "common"},
    {"host", "Host information (default)", "common"},
    {"platform", "Platforms flashed on the device", "common"},
    {"telemetry", "Telemetry data for the device", "hidden"},
    {"preemption", "Preemption telemetry data for the device", "hidden"},
    {"clocks", "Clock frequency information", "hidden"},
    {"debug", "Debug configuration settings for the device", "hidden"}
  };

  validate_test_desc = {
    {"aie-reconfig-overhead", "Run end-to-end array reconfiguration overhead through shim DMA", "hidden"},
    {"all", "All applicable validate tests will be executed (default)", "common"},
    {"runlist-latency", "Run end-to-end latency test using runlist", "hidden"},
    {"runlist-throughput", "Run end-to-end throughput test using runlist", "hidden"},
    {"df-bw", "Run bandwidth test on data fabric", "hidden"},
    {"gemm", "Measure the TOPS value of GEMM INT8operations", "common"},
    {"sanity", "Run a small model and validate sanity of the device", "hidden"},
    {"latency", "Run end-to-end latency test", "common"},
    {"quick", "Run a subset of four tests: \n1. latency \n2. throughput \n3. runlist-latency \n4. runlist-throughput", "hidden"},
    {"tct-all-col", "Measure average TCT processing time for all columns", "hidden"},
    {"tct-one-col", "Measure average TCT processing time for one column", "hidden"},
    {"throughput", "Run end-to-end throughput test", "common"},
    {"temporal-sharing-overhead", "Run end-to-end temporal sharing overhead test", "hidden"},
    {"preemption-overhead", "Measure preemption overhead at noop and memtile levels", "hidden"}
  };
}

} // namespace xrt_core::smi::ryzen
