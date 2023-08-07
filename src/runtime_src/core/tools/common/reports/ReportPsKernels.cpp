// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportPsKernels.h"

#include "tools/common/PsKernelUtilities.h"
#include "tools/common/Table2D.h"

#include "core/common/query_requests.h"

#include <boost/property_tree/json_parser.hpp>

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
    // Validate if the device can support ps kernels
    if (!xrt_core::device_query<xrt_core::query::is_versal>(device))
      return;
    boost::property_tree::ptree p = get_ps_instance_data(device);
    pt.add_child("instance_data", p);
  }
  catch (const xrt_core::query::exception&) {}
}

void 
ReportPsKernels::writeReport( const xrt_core::device* /*_pDevice*/,
                              const boost::property_tree::ptree& pt,
                              const std::vector<std::string>& /*_elementsFilter*/,
                              std::ostream & output) const
{
  output << "PS Kernels\n";
  if (pt.empty()) {
    output << "  Report not valid for specified device\n";
    return;
  }

  output << "  APU Image\n";
  auto metadata_fmt = boost::format("%s%-22s: %s\n");
  const auto& os_data_pt = pt.get_child("instance_data.apu_image");
  const auto os_data_pad = std::string(4, ' ');
  output << metadata_fmt % os_data_pad % "System name" % os_data_pt.get<std::string>("sysname");
  output << metadata_fmt % os_data_pad % "Release" % os_data_pt.get<std::string>("release");
  output << metadata_fmt % os_data_pad % "Version" % os_data_pt.get<std::string>("version");
  output << metadata_fmt % os_data_pad % "Machine" % os_data_pt.get<std::string>("machine");
  output << metadata_fmt % os_data_pad % "Distribution" % os_data_pt.get<std::string>("distribution");
  output << metadata_fmt % os_data_pad % "Model" % os_data_pt.get<std::string>("model");
  output << metadata_fmt % os_data_pad % "Cores" % os_data_pt.get<std::string>("cores");
  output << metadata_fmt % os_data_pad % "Total Memory" % os_data_pt.get<std::string>("mem_total");
  output << metadata_fmt % os_data_pad % "Available Memory" % os_data_pt.get<std::string>("mem_available");
  output << metadata_fmt % os_data_pad % "Address Space" % os_data_pt.get<std::string>("address_space");
  output << "\n";

  // Loop through each kernel instance
  output << "  PS Kernel Instances\n";
  for (const auto& kernel_list : pt.get_child("instance_data.ps_kernel_instances")) {
    const auto& kernel_instance_ptree = kernel_list.second;
    std::string kernel_name = kernel_list.first;
    const size_t kernel_space_offset = 2;
    const auto kernel_space_string = std::string(kernel_space_offset, ' ');
    const auto output_kernel_name = boost::format("%sKernel: %s\n") % kernel_space_string % kernel_name;
    output << boost::format("%s%s\n") % kernel_space_string % std::string(output_kernel_name.size(), '-');
    output << output_kernel_name;
    output << boost::format("%s%s\n") % kernel_space_string % std::string(output_kernel_name.size(), '-');

    // Iterate through the instances that implement the above kernel
    size_t instance_index = 0;
    for (const auto& ps_instance : kernel_instance_ptree) {
      const auto& ps_ptree = ps_instance.second;
      const auto& data_pt = ps_ptree.get_child("process_info");

      std::vector<boost::property_tree::ptree> instance_data;
      for (const auto& a : data_pt)
        instance_data.push_back(a.second);

      // Format the process status for each instance into a table
      Table2D::HeaderData name_type = {"Name", Table2D::Justification::left};
      Table2D::HeaderData value_type = {"Value", Table2D::Justification::left};
      std::vector<Table2D::HeaderData> table_headers = {name_type, value_type, name_type, value_type, name_type, value_type};
      Table2D instance_table(table_headers);

      // Add the instance data into the above table
      const size_t max_col_length = 20; // Maximum number of entries per column
      for (size_t data_index = 0; data_index < max_col_length; data_index++) {
        std::vector<std::string> entry_data;
        // Split entries into 3 groupings of name value pairings
        entry_data.push_back(instance_data[data_index].get<std::string>("name"));
        entry_data.push_back(instance_data[data_index].get<std::string>("value"));
        if (data_index + (2 * max_col_length) < instance_data.size()) {
          entry_data.push_back(instance_data[data_index + max_col_length].get<std::string>("name"));
          entry_data.push_back(instance_data[data_index + max_col_length].get<std::string>("value"));
          entry_data.push_back(instance_data[data_index + 2 * max_col_length].get<std::string>("name"));
          entry_data.push_back(instance_data[data_index + 2 * max_col_length].get<std::string>("value"));
        } else if (data_index + max_col_length < instance_data.size()) {
          entry_data.push_back(instance_data[data_index + max_col_length].get<std::string>("name"));
          entry_data.push_back(instance_data[data_index + max_col_length].get<std::string>("value"));
        }
        // Populate the missing entries with empty space
        while (entry_data.size() < table_headers.size())
          entry_data.push_back("");

        instance_table.addEntry(entry_data);
      }

      // Output the instance data
      const size_t name_pad = kernel_space_offset + 2;
      const size_t data_pad = name_pad + 2;
      // Use the maximum offset when generating the seperating line
      const auto instance_divider = std::string((data_pad - name_pad) + instance_table.getTableCharacterLength(), '=');
      const auto name_pad_str = std::string(name_pad, ' ');
      output << boost::format("%s%s\n") % name_pad_str % instance_divider;
      output << boost::format("%s[%d] %s\n") % name_pad_str % instance_index % ps_ptree.get<std::string>("name");
      const auto data_pad_str = std::string(data_pad, ' ');
      output << metadata_fmt % data_pad_str % "Kernel" % ps_ptree.get<std::string>("kernel");
      output << metadata_fmt % data_pad_str % "CU Address" % ps_ptree.get<std::string>("cu_address");
      output << metadata_fmt % data_pad_str % "CU Index" % ps_ptree.get<std::string>("cu_index");
      output << metadata_fmt % data_pad_str % "Protocol" % ps_ptree.get<std::string>("protocol");
      output << metadata_fmt % data_pad_str % "Interrupt Compatible" % ps_ptree.get<std::string>("interrupt_compatible");
      output << metadata_fmt % data_pad_str % "Resettable" % ps_ptree.get<std::string>("resettable");
      output << metadata_fmt % data_pad_str % "Argument Count" % ps_ptree.get<std::string>("argument_count");
      output << "\n"; // Seperate instance metadata from process info
      output << boost::format("%s%s\n") % data_pad_str % "Process Properties";
      output << boost::format("%s\n") % instance_table.toString(data_pad_str);
      instance_index++;
    }
  }
  output << std::endl;
}
