// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#include "BuiltInPsKernels.h"

#include "core/common/query_requests.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

#include <vector>

#include <boost/property_tree/json_parser.hpp>


static void
get_all_instance_data(const xrt_core::device * _pDevice, boost::property_tree::ptree &pt)
{
  static size_t COUNT = 4096;
  // TODO this will be removed when the PS kernel is built in
  std::string b_file = "/ps_validate_bandwidth.xclbin";
  std::string binaryfile = "/opt/xilinx/firmware/vck5000/gen4x8-qdma/base/test" + b_file;
  auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(_pDevice));
  auto device = xrt::device {bdf};
  auto uuid = device.load_xclbin(binaryfile);
  // end TODO

  // Always get a kernel reference to the build in kernel. Choose a better name though...
  auto hello_world = xrt::kernel(device, uuid.get(), "hello_world");

  // Format the amount of data depending on the number of programmed ps kernels
  auto ps_data = xrt_core::device_query<xrt_core::query::kds_scu_info>(_pDevice);
  const size_t DATA_SIZE = COUNT * sizeof(char) * ps_data.size();
  auto bo0 = xrt::bo(device, DATA_SIZE, hello_world.group_id(0));
  auto bo0_map = bo0.map<char*>();
  std::fill(bo0_map, bo0_map + COUNT, 0);

  bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);

  auto run = hello_world(bo0, DATA_SIZE);
  run.wait();

  //Get the output;
  bo0.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0);

  // Parse the output into a useful format
  std::istringstream json(bo0_map);
  boost::property_tree::read_json(json, pt);
}

static std::vector<const boost::property_tree::ptree*>
get_sorted_instance_list(const boost::property_tree::ptree& pt)
{
  std::vector<const boost::property_tree::ptree*> instance_list;
  // Sorting lambda
  // Sort based on ps kernel instance kernel and instance names
  auto ptree_sorting = [](const boost::property_tree::ptree* a, const boost::property_tree::ptree* b) {
    // First sort based on kernel name
    const auto& a_name = a->get<std::string>("ps_instance_meta.Kernel name");
    const auto& b_name = b->get<std::string>("ps_instance_meta.Kernel name");
    const auto val = a_name.compare(b_name);
    if (val < 0)
      return true;
    else if (val > 0)
      return false;

    // If the kernel names are equal sort based on the instance name
    const auto& a_i_name = a->get<std::string>("ps_instance_meta.Instance(CU) name");
    const auto& b_i_name = b->get<std::string>("ps_instance_meta.Instance(CU) name");
    const auto val_i = a_i_name.compare(b_i_name);
    if (val_i <= 0)
      return true;
    else
      return false;
  };

  // Add each instance to a list for sorting
  for(const auto& ps_instance : pt.get_child("ps_instances"))
    instance_list.push_back(&ps_instance.second);
  std::sort(instance_list.begin(), instance_list.end(), ptree_sorting);

  return instance_list;
}


static boost::property_tree::ptree
parse_instance(const boost::property_tree::ptree& instance_pt)
{
  // Parse the instance status data
  boost::property_tree::ptree parsed_pt;

  // Get metadata like kernel name and instance name
  parsed_pt.add_child("metadata", instance_pt.get_child("ps_instance_meta"));

  // Get skd related status information like PID
  parsed_pt.add_child("status", instance_pt.get_child("ps_instance_status"));

  // Get and sort the process data
  const auto& data_pt = instance_pt.get_child("process_status");
  std::vector<const boost::property_tree::ptree*> instance_data;
  for (const auto& a : data_pt)
    instance_data.push_back(&a.second);

  auto ptree_sorting = [](const boost::property_tree::ptree* a, const boost::property_tree::ptree* b) {
    const auto& a_name = a->get<std::string>("name");
    const auto& b_name = b->get<std::string>("name");
    const auto val = a_name.compare(b_name);
    if (val <= 0)
      return true;
    else
      return false; 
  };
  std::sort(instance_data.begin(), instance_data.end(), ptree_sorting);
  boost::property_tree::ptree status_pt;
  for (const auto& item : instance_data)
    status_pt.push_back(std::make_pair("", *item));
  parsed_pt.add_child("process_data", status_pt);

  return parsed_pt;
}

boost::property_tree::ptree
get_ps_instance_data(const xrt_core::device* device)
{
  boost::property_tree::ptree all_instance_data;
  get_all_instance_data(device, all_instance_data);

  // Parse the returned data
  // First sort all of the instances
  const auto instance_list = get_sorted_instance_list(all_instance_data);

  // Parse each instance into trees based on their kernel names
  boost::property_tree::ptree sorted_instance_tree;
  for (const auto& ps_instance_ptr : instance_list) {
    const auto& kernel_name = ps_instance_ptr->get<std::string>("ps_instance_meta.Kernel name");

    // Parse the current instance into the approved schema
    const boost::property_tree::ptree parsed_pt = parse_instance(*ps_instance_ptr);

    // Make sure there is a seperate tree for each kernel
    boost::property_tree::ptree empty_tree;
    auto& kernel_tree = sorted_instance_tree.get_child(kernel_name, empty_tree);
    // Check if the tree is empty before adding the new array entry
    const auto is_tree_empty = kernel_tree.empty();
    kernel_tree.push_back(std::make_pair("", parsed_pt));
    if (is_tree_empty)
      sorted_instance_tree.add_child(kernel_name, kernel_tree);
  }

  // Format the data into the controlled schema
  boost::property_tree::ptree parsed_kernel_data;
  parsed_kernel_data.add_child("schema_version", all_instance_data.get_child("schema_version"));
  parsed_kernel_data.add_child("os", all_instance_data.get_child("os_data"));
  parsed_kernel_data.add_child("ps_instances", sorted_instance_tree);
  return parsed_kernel_data;
}
