// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportPsKernels.h"

#include "BuiltInPsKernels.h"
#include "Table2D.h"

#include "core/common/query_requests.h"

#include <boost/property_tree/json_parser.hpp>

namespace qr = xrt_core::query;

void
ReportPsKernels::getPropertyTreeInternal(const xrt_core::device * device,
                                              boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(device, pt);
}

void 
ReportPsKernels::getPropertyTree20202( const xrt_core::device * device,
                                           boost::property_tree::ptree &pt) const
{
  try {
    boost::property_tree::ptree p = get_ps_instance_data(device);
    pt.add_child("instance_data", p);
  }
  catch (const qr::exception&) {}
}

void 
ReportPsKernels::writeReport( const xrt_core::device* /*_pDevice*/,
                              const boost::property_tree::ptree& pt,
                              const std::vector<std::string>& /*_elementsFilter*/,
                              std::ostream & output) const
{
  output << "Operating System:\n";
  for (const auto& os_data : pt.get_child("instance_data.os"))
      output << boost::format("  %s: %s\n") % os_data.first % os_data.second.data();

  // Loop through each kernel instance
  output << "PS Kernel Instances:\n";
  for (const auto& kernel_list : pt.get_child("instance_data.ps_instances")) {
    const auto& kernel_instance_ptree = kernel_list.second;
    std::string kernel_name = kernel_list.first;
    const size_t hyphen_length = 40;
    output << std::string(hyphen_length, '-') << std::endl;
    output << boost::format("Kernel Name: %s\n") % kernel_name;

    // Iterate through the instances that implement the above kernel
    for (const auto& ps_instance : kernel_instance_ptree) {
      const auto& ps_ptree = ps_instance.second;
      const auto& data_pt = ps_ptree.get_child("process_data");

      std::vector<boost::property_tree::ptree> instance_data;
      for (const auto& a : data_pt)
        instance_data.push_back(a.second);

      // Format the process status for each instance into a table
      Table2D::HeaderData name_type = {"Name", Table2D::Justification::left};
      Table2D::HeaderData value_type = {"Value", Table2D::Justification::left};
      std::vector<Table2D::HeaderData> table_headers = {name_type, value_type, name_type, value_type, name_type, value_type};
      Table2D instance_table(table_headers);

      const size_t max_table_length = 20;
      for (size_t data_index = 0; data_index < max_table_length; data_index++) {
        std::vector<std::string> entry_data;
        if (data_index + (2 * max_table_length) < instance_data.size()) {
          entry_data.push_back(instance_data[data_index].get<std::string>("name"));
          entry_data.push_back(instance_data[data_index].get<std::string>("value"));
          entry_data.push_back(instance_data[data_index + max_table_length].get<std::string>("name"));
          entry_data.push_back(instance_data[data_index + max_table_length].get<std::string>("value"));
          entry_data.push_back(instance_data[data_index + 2 * max_table_length].get<std::string>("name"));
          entry_data.push_back(instance_data[data_index + 2 * max_table_length].get<std::string>("value"));
        } else if (data_index + max_table_length < instance_data.size()) {
          entry_data.push_back(instance_data[data_index].get<std::string>("name"));
          entry_data.push_back(instance_data[data_index].get<std::string>("value"));
          entry_data.push_back(instance_data[data_index + max_table_length].get<std::string>("name"));
          entry_data.push_back(instance_data[data_index + max_table_length].get<std::string>("value"));
          entry_data.push_back("");
          entry_data.push_back("");
        } else {
          entry_data.push_back(instance_data[data_index].get<std::string>("name"));
          entry_data.push_back(instance_data[data_index].get<std::string>("value"));
          entry_data.push_back("");
          entry_data.push_back("");
          entry_data.push_back("");
          entry_data.push_back("");
        }
        instance_table.addEntry(entry_data);
      }

      // Output the instance data
      std::string instance_name = ps_ptree.get<std::string>("metadata.Instance(CU) name");
      output << boost::format("  Instance name: %s\n") % instance_name;
      output << "    Process Status:\n";
      output << boost::format("%s\n") % instance_table.to_string("    ");
    }
  }
  output << std::endl;
}
