// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportPreemption.h"
#include "core/common/info_telemetry.h"
#include "tools/common/Table2D.h"

// 3rd Party Library - Include Files
#include <vector>

using bpt = boost::property_tree::ptree;

void
ReportPreemption::getPropertyTreeInternal(const xrt_core::device* dev,
                                         bpt& pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void
ReportPreemption::getPropertyTree20202(const xrt_core::device* dev,
                                      bpt& pt) const
{
  // There can only be 1 root node
  pt = xrt_core::telemetry::preemption_telemetry_info(dev);
}

static std::string
generate_preemption_string(const bpt& pt)
{
  std::stringstream ss;

  std::vector<Table2D::HeaderData> preempt_headers = {
    {"FW TID", Table2D::Justification::left},
    {"Ctx ID", Table2D::Justification::left},
    {"Layer Events", Table2D::Justification::left},
    {"Frame Events", Table2D::Justification::left},
  };
  Table2D preemption_table(preempt_headers);

  for (const auto& [name, user_task] : pt) {
    const std::vector<std::string> rtos_data = {
      user_task.get<std::string>("fw_tid"),
      user_task.get<std::string>("ctx_index"),
      user_task.get<std::string>("layer_events"),
      user_task.get<std::string>("frame_events"),
    };
    preemption_table.addEntry(rtos_data);
  }

  ss << preemption_table.toString("  ") << "\n";

  return ss.str();
}

void
ReportPreemption::writeReport(const xrt_core::device* ,
                             const bpt& pt,
                             const std::vector<std::string>&,
                             std::ostream& _output) const
{
  const bpt empty_ptree;
  bpt telemetry_array = pt.get_child("telemetry", empty_ptree);
  _output << "Preemption Telemetry Data\n";
  _output << generate_preemption_string(telemetry_array);
  _output << std::endl;
}
