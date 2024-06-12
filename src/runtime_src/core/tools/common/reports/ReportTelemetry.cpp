// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportTelemetry.h"

#include "core/common/info_telemetry.h"
#include "tools/common/Table2D.h"

#include <vector>

const boost::property_tree::ptree empty_ptree;

void
ReportTelemetry::getPropertyTreeInternal(const xrt_core::device* dev,
                                         boost::property_tree::ptree& pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void
ReportTelemetry::getPropertyTree20202(const xrt_core::device* dev,
                                      boost::property_tree::ptree& pt) const
{
  // There can only be 1 root node
  pt = xrt_core::telemetry::telemetry_info(dev);
}

static std::string
generate_opcode_string(const boost::property_tree::ptree& pt)
{
  std::stringstream ss;

  const std::vector<Table2D::HeaderData> opcode_headers = {
    {"Opcode", Table2D::Justification::left},
    {"Receive Count", Table2D::Justification::left}
  };
  Table2D opcode_table(opcode_headers);

  int index = 0;
  boost::property_tree::ptree opcodes = pt.get_child("opcodes", empty_ptree);
  for (const auto& [name, opcode] : opcodes) {
    std::vector<std::string> opcode_data = {
      std::to_string(index),
      std::to_string(opcode.get<uint64_t>("received_count"))
    };
    opcode_table.addEntry(opcode_data);
    index++;
  }

  ss << opcode_table.toString("  ");

  return ss.str();
}

void
ReportTelemetry::writeReport(const xrt_core::device* /*_pDevice*/,
                             const boost::property_tree::ptree& pt,
                             const std::vector<std::string>& /*_elementsFilter*/,
                             std::ostream& _output) const
{
  _output << "Telemetry\n";

  boost::property_tree::ptree telemetry_pt = pt.get_child("telemetry", empty_ptree);
  if (telemetry_pt.empty()) {
    _output << "  No telemetry information available\n\n";
    return;
  }
  _output << generate_opcode_string(telemetry_pt);
  _output << std::endl;
}
