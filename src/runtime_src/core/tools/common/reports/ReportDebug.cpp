// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportDebug.h"

#include "core/common/query_requests.h"

#include <boost/format.hpp>
#include <functional>

namespace xq = xrt_core::query;

namespace {

std::string
enabled_status(uint32_t value)
{
  return value == 1 ? "enabled" : "disabled";
}

std::string
format_line(const boost::property_tree::ptree& setting)
{
  const auto status = setting.get<std::string>("status");
  const auto detail = setting.get<std::string>("detail", "");
  if (detail.empty())
    return status;
  return boost::str(boost::format("%s (%s)") % status % detail);
}

void
add_setting(boost::property_tree::ptree& parent, const std::string& name,
            const std::function<void(boost::property_tree::ptree&)>& populate)
{
  boost::property_tree::ptree node;
  try {
    populate(node);
  }
  catch (const std::exception&) {
    node.put("status", "not supported");
  }
  parent.add_child(name, node);
}

void
populate_debug(const xrt_core::device* dev, boost::property_tree::ptree& debug_pt)
{
  add_setting(debug_pt, "firmware_log", [&](boost::property_tree::ptree& node) {
    const auto fw = xrt_core::device_query<xq::firmware_log_state>(dev);
    node.put("status", enabled_status(fw.action));
    if (fw.action == 1)
      node.put("detail", boost::str(boost::format("log level %u") % fw.log_level));
  });

  add_setting(debug_pt, "event_trace", [&](boost::property_tree::ptree& node) {
    const auto et = xrt_core::device_query<xq::event_trace_state>(dev);
    node.put("status", enabled_status(et.action));
    if (et.action == 1 && et.categories != 0)
      node.put("detail", boost::str(boost::format("categories 0x%x") % et.categories));
  });

  add_setting(debug_pt, "auto_coredump", [&](boost::property_tree::ptree& node) {
    const auto coredump = xrt_core::device_query<xq::auto_coredump>(dev);
    node.put("status", enabled_status(coredump));
  });
}

} // namespace

void
ReportDebug::getPropertyTreeInternal(const xrt_core::device* dev, boost::property_tree::ptree& pt) const
{
  getPropertyTree20202(dev, pt);
}

void
ReportDebug::getPropertyTree20202(const xrt_core::device* dev, boost::property_tree::ptree& pt) const
{
  boost::property_tree::ptree debug_pt;
  populate_debug(dev, debug_pt);
  pt.add_child("debug", debug_pt);
}

void
ReportDebug::writeReport(const xrt_core::device* /*dev*/, const boost::property_tree::ptree& pt,
                         const std::vector<std::string>& /*elementsFilter*/,
                         std::ostream& output) const
{
  const auto& debug_pt = pt.get_child("debug");

  output << "Debug Configurations\n";
  output << boost::format("  %-20s: %s\n") % "Firmware Log"
           % format_line(debug_pt.get_child("firmware_log"));
  output << boost::format("  %-20s: %s\n") % "Event Trace"
           % format_line(debug_pt.get_child("event_trace"));
  output << boost::format("  %-20s: %s\n") % "Auto Coredump"
           % format_line(debug_pt.get_child("auto_coredump"));
  output << "\n";
}
