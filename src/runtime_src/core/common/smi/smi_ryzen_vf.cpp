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
  examine_report_desc = {
    {"all", "All known reports are produced", "common"},
    {"host", "Host information", "common"},
    {"platform", "Platforms flashed on the device", "common"},
    {"telemetry", "Telemetry data for the device", "common"},
    {"clocks", "Clock frequency information", "hidden"},
    {"debug", "Debug configuration settings for the device", "hidden"}
  };
}

} // namespace xrt_core::smi::ryzen
