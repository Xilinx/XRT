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

  examine_report_desc = {
    {"all", "All known reports are produced", "common"},
    {"host", "Host information", "common"},
    {"platform", "Platforms flashed on the device", "common"},
    {"telemetry", "Telemetry data for the device", "common"},
    {"preemption", "Preemption telemetry data for the device", "common"},
    {"clocks", "Clock frequency information", "hidden"},
    {"debug", "Debug configuration settings for the device", "hidden"}
  };
}

subcommand
config_gen_npu3_pf::create_examine_subcommand()
{
  // context-health is VF-only (requires a user DRM handle / live contexts).
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

  return {"examine", "This command will 'examine' the state of the system/device and will generate a report of interest in a text or JSON format.", "common", std::move(examine_suboptions)};
}

subcommand
config_gen_npu3_pf::create_configure_subcommand()
{
  std::map<std::string, std::shared_ptr<option>> configure_suboptions;
  configure_suboptions.emplace("device", std::make_shared<option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  configure_suboptions.emplace("help", std::make_shared<option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  configure_suboptions.emplace("force-preemption", std::make_shared<option>("force-preemption", "", "Force enable|disable and see status of preemption", "hidden", "", "string", true));
  configure_suboptions.emplace("event-trace", std::make_shared<option>("event-trace", "", "Enable|disable event tracing", "hidden", "", "string", true));
  configure_suboptions.emplace("firmware-log", std::make_shared<option>("firmware-log", "", "Enable|disable firmware logging", "hidden", "", "string", true));
  configure_suboptions.emplace("auto-coredump", std::make_shared<option>("auto-coredump", "", "Enable|disable automatic coredump on error", "hidden", "", "string", true));

  return {"configure", "Device and host configuration", "common", std::move(configure_suboptions)};
}

} // namespace xrt_core::smi::ryzen
