// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportAiePartitions.h"

#include "core/common/info_aie.h"
#include "Table2D.h"

#include <vector>

void
ReportAiePartitions::
getPropertyTreeInternal(const xrt_core::device * _pDevice, 
                        boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportAiePartitions::
getPropertyTree20202(const xrt_core::device * _pDevice, 
                     boost::property_tree::ptree &_pt) const
{
  xrt::device device(_pDevice->get_device_id());
  std::stringstream ss;
  ss << device.get_info<xrt::info::device::aie_partitions>();
  boost::property_tree::read_json(ss, _pt);
}

void 
ReportAiePartitions::
writeReport(const xrt_core::device* /*_pDevice*/,
            const boost::property_tree::ptree& _pt,
            const std::vector<std::string>& _elementsFilter,
            std::ostream & _output) const
{
  _output << "AIE Partitions\n";
  boost::property_tree::ptree empty_ptree;
  const boost::property_tree::ptree pt_partitions = _pt.get_child("aie_partitions", empty_ptree);
  if (pt_partitions.empty()) {
    _output << "  AIE Partition information unavailable\n\n";
    return;
  }

  for (const auto& pt_partition : pt_partitions) {
    const auto& partition = pt_partition.second;
    const auto start_col = partition.get<uint64_t>("start_col");
    const auto num_cols = partition.get<uint64_t>("num_cols");
    std::string column_string;
    for (uint64_t i = 0; i < num_cols; i++) {
      column_string += std::to_string(start_col + i);
      if (i < num_cols - 1)
        column_string += ", ";
    }
    _output << "  Columns: [" << column_string << "]\n";

    const std::vector<Table2D::HeaderData> table_headers = {
      {"Slot ID", Table2D::Justification::left},
      {"Xclbin UUID", Table2D::Justification::left},
      {"Usage Count", Table2D::Justification::left},
      {"Migration Count", Table2D::Justification::left},
      {"BO Sync Count", Table2D::Justification::left}
    };
    Table2D context_table(table_headers);

    _output << "    HW Contexts:\n";
    for (const auto& pt_hw_context : partition.get_child("hw_contexts", empty_ptree)) {
      const auto& hw_context = pt_hw_context.second;
      const std::vector<std::string> entry_data = {
        hw_context.get<std::string>("slot_id"),
        hw_context.get<std::string>("xclbin_uuid"),
        hw_context.get<std::string>("usage_count"),
        hw_context.get<std::string>("migration_count"),
        hw_context.get<std::string>("bo_sync_count")
      };
      context_table.addEntry(entry_data);
    }
    _output << context_table.toString("      ") << "\n";
  }
  _output << "\n";
}

