// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportAieLoad.h"

#include "core/common/query_requests.h"

#include <boost/format.hpp>
#include <limits>

void
ReportAieLoad::getPropertyTreeInternal(const xrt_core::device* dev,
                                       boost::property_tree::ptree& pt) const
{
  getPropertyTree20202(dev, pt);
}

void
ReportAieLoad::getPropertyTree20202(const xrt_core::device* dev,
                                    boost::property_tree::ptree& pt) const
{
  boost::property_tree::ptree node;

  try {
    const xrt_core::query::aie_load::args args{m_duration_ms};
    const auto data = xrt_core::device_query<xrt_core::query::aie_load>(dev, args);

    const bool unavailable = (data.load_percent == std::numeric_limits<uint32_t>::max());
    node.put("load_percent",          unavailable ? "N/A" : std::to_string(data.load_percent));
    node.put("timestamp_ms",          data.timestamp_ms);
    node.put("operations_per_second", data.operations_per_second);

    boost::property_tree::ptree pt_counters;
    for (size_t i = 0; i < xrt_core::query::aie_load::num_activity_counters; i++) {
      boost::property_tree::ptree pt_counter;
      pt_counter.put("", data.activity_counters[i]);
      pt_counters.push_back({"", pt_counter});
    }
    node.add_child("activity_counters", pt_counters);
  }
  catch (const xrt_core::query::exception& e) {
    node.put("error", e.what());
  }

  pt.add_child("aie_load", node);
}

void
ReportAieLoad::writeReport(const xrt_core::device* /*dev*/,
                           const boost::property_tree::ptree& pt,
                           const std::vector<std::string>& /*elementsFilter*/,
                           std::ostream& output) const
{
  const auto& node = pt.get_child("aie_load");

  // If the query failed, report the error and return.
  auto error = node.get_optional<std::string>("error");
  if (error) {
    output << "AIE Load: " << *error << "\n\n";
    return;
  }

  output << "AIE Load\n";

  const auto load = node.get<std::string>("load_percent");
  if (load == "N/A")
    output << boost::format("  %-25s: N/A (AIE off, gated, or counters unavailable)\n") % "Utilization";
  else
    output << boost::format("  %-25s: %s%%\n") % "Utilization" % load;

  output << boost::format("  %-25s: %s ops/s\n") % "Operations/Second"
            % node.get<std::string>("operations_per_second");
  output << boost::format("  %-25s: %s ms\n") % "FW Timestamp"
            % node.get<std::string>("timestamp_ms");

  output << "  Activity Counters:\n";
  int idx = 0;
  for (const auto& [name, counter] : node.get_child("activity_counters"))
    output << boost::format("    [%d]: %s\n") % idx++ % counter.get_value<std::string>();

  output << "\n";
}
