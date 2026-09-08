// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.

#define XRT_CORE_COMMON_SOURCE

#include "core/common/smi/smi_ryzen_pf.h"

using namespace xrt_core::smi;

namespace xrt_core::smi::ryzen {

config_gen_npu3_pf::
config_gen_npu3_pf()
  : config_gen_npu3()
{
  validate_test_desc = {};
}

subcommand
config_gen_npu3_pf::create_configure_subcommand()
{
  std::map<std::string, std::shared_ptr<option>> configure_suboptions;
  configure_suboptions.emplace("device", std::make_shared<option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  configure_suboptions.emplace("help", std::make_shared<option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  configure_suboptions.emplace("event-trace", std::make_shared<option>("event-trace", "", "Enable|disable event tracing", "hidden", "", "string", true));
  configure_suboptions.emplace("firmware-log", std::make_shared<option>("firmware-log", "", "Enable|disable firmware logging", "hidden", "", "string", true));
  configure_suboptions.emplace("auto-coredump", std::make_shared<option>("auto-coredump", "", "Enable|disable automatic coredump on error", "hidden", "", "string", true));

  return {"configure", "Device and host configuration", "common", std::move(configure_suboptions)};
}

} // namespace xrt_core::smi::ryzen
