// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// Local - Include Files

#define XRT_CORE_COMMON_SOURCE

#include "core/common/smi.h"
#include "SmiDefault.h"

#include <boost/property_tree/ptree.hpp>

// 3rd Party Library - Include Files
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <tuple>

namespace xrt_smi_default {

xrt_core::smi::subcommand
create_validate_subcommand() 
{
    std::map<std::string, std::shared_ptr<xrt_core::smi::option>> validate_suboptions;
  validate_suboptions.emplace("device", std::make_shared<xrt_core::smi::option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  validate_suboptions.emplace("format", std::make_shared<xrt_core::smi::option>("format", "f", "Report output format. Valid values are:\n"
                                "\tJSON        - Latest JSON schema\n"
                                "\tJSON-2020.2 - JSON 2020.2 schema", "common", "JSON", "string"));
  validate_suboptions.emplace("output", std::make_shared<xrt_core::smi::option>("output", "o", "Direct the output to the given file", "common", "", "string"));
  validate_suboptions.emplace("help", std::make_shared<xrt_core::smi::option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  validate_suboptions.emplace("run", std::make_shared<xrt_core::smi::option>("run", "r", "Run a subset of the test suite\n",
                              "common", "",  "array"));

  return {"validate", "Validates the given device by executing the platform's validate executable", "common", std::move(validate_suboptions)};
}

xrt_core::smi::subcommand
create_examine_subcommand() 
{
  std::vector<xrt_core::smi::basic_option> examine_report_desc = {
    {"host", "Host information", "common"},
  };
  std::map<std::string, std::shared_ptr<xrt_core::smi::option>> examine_suboptions; 
  examine_suboptions.emplace("device", std::make_shared<xrt_core::smi::option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  examine_suboptions.emplace("help", std::make_shared<xrt_core::smi::option>("help", "h", "Help to use this sub-command", "common", "", "none"));
  examine_suboptions.emplace("report", std::make_shared<xrt_core::smi::listable_description_option>("report", "r", "The type of report to be produced. Reports currently available are:\n", "common", "", "array", examine_report_desc));
  examine_suboptions.emplace("element", std::make_shared<xrt_core::smi::option>("element", "e", "Filters individual elements(s) from the report. Format: '/<key>/<key>/...'", "hidden", "", "array"));

  return {"examine", "This command will 'examine' the state of the system/device and will generate a report of interest in a text or JSON format.", "common", std::move(examine_suboptions)};
}

xrt_core::smi::subcommand
create_configure_subcommand() 
{
  std::map<std::string, std::shared_ptr<xrt_core::smi::option>> configure_suboptions;
  configure_suboptions.emplace("device", std::make_shared<xrt_core::smi::option>("device", "d", "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest", "common", "", "string"));
  configure_suboptions.emplace("help", std::make_shared<xrt_core::smi::option>("help", "h", "Help to use this sub-command", "common", "", "none"));

  return {"configure", "Device and host configuration", "common", std::move(configure_suboptions)};
}

std::string 
get_default_smi_config() 
{
  auto smi_instance = xrt_core::smi::instance();
  smi_instance->add_subcommand("validate", create_validate_subcommand());
  smi_instance->add_subcommand("examine", create_examine_subcommand());
  smi_instance->add_subcommand("configure", create_configure_subcommand());

  return smi_instance->build_smi_config();
}
} // namespace xrt_smi_default
