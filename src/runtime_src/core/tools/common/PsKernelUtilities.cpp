// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#include "PsKernelUtilities.h"

#include "XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/common/query_requests.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

#include <cctype>
#include <regex>
#include <string>
#include <vector>

#include <boost/property_tree/json_parser.hpp>
namespace pt = boost::property_tree;

static void
get_all_instance_data(const xrt_core::device * /*_pDevice*/, pt::ptree &/*pt*/)
{
  // // TODO put request logic in here for ps kernel data query
  // static size_t COUNT = 4096;
  // // TODO this will be removed when the PS kernel is built in
  // std::string b_file = "/ps_validate_bandwidth.xclbin";
  // std::string binaryfile = "/opt/xilinx/firmware/vck5000/gen4x8-qdma/base/test" + b_file;
  // auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(_pDevice));
  // auto device = xrt::device {bdf};
  // auto uuid = device.load_xclbin(binaryfile);
  // // end TODO

  // // Always get a kernel reference to the build in kernel. Choose a better name though...
  // auto data_kernel = xrt::kernel(device, uuid.get(), "get_ps_kernel_data");

  // // Format the amount of data depending on the number of programmed ps kernels
  // auto ps_data = xrt_core::device_query<xrt_core::query::kds_scu_info>(_pDevice);
  // const size_t DATA_SIZE = COUNT * sizeof(char) * ps_data.size();
  // auto bo0 = xrt::bo(device, DATA_SIZE, data_kernel.group_id(0));
  // auto bo0_map = bo0.map<char*>();
  // std::fill(bo0_map, bo0_map + COUNT, 0);

  // bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);

  // auto run = data_kernel(bo0, DATA_SIZE, true);
  // run.wait();

  // //Get the output;
  // bo0.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0);

  // // Parse the output into a useful format
  // std::istringstream json(bo0_map);
  // pt::read_json(json, pt);
}

static std::vector<const pt::ptree*>
get_sorted_instance_list(const pt::ptree& pt)
{
  std::vector<const pt::ptree*> instance_list;
  // Sorting lambda
  // Sort based on ps kernel instance kernel and instance names
  auto ptree_sorting = [](const pt::ptree* a, const pt::ptree* b) {
    // First sort based on kernel name
    const auto& a_name = a->get<std::string>("kernel");
    const auto& b_name = b->get<std::string>("kernel");
    const auto val = a_name.compare(b_name);
    if (val < 0)
      return true;
    if (val > 0)
      return false;

    // If the kernel names are equal sort based on the instance name
    const auto& a_i_name = a->get<std::string>("name");
    const auto& b_i_name = b->get<std::string>("name");
    const auto val_i = a_i_name.compare(b_i_name);
    if (val_i <= 0)
      return true;

    return false;
  };

  // Add each instance to a list for sorting
  for (const auto& ps_instance : pt.get_child("ps_kernel_instances"))
    instance_list.push_back(&ps_instance.second);
  std::sort(instance_list.begin(), instance_list.end(), ptree_sorting);

  return instance_list;
}

static char to_uppercase(const char letter)
{
  int letter_val = static_cast<int>(letter);
  return static_cast<char>(std::toupper(letter_val));
}

static pt::ptree
parse_instance(const pt::ptree& instance_pt)
{
  // Parse the instance status data
  pt::ptree parsed_pt;

  // Transfer metadata except process info array
  for (const auto& item : instance_pt)
    if (!boost::equal(item.first, std::string("process_info")))
      parsed_pt.put(item.first, item.second.data());

  // Sorting process info
  const auto& data_pt = instance_pt.get_child("process_info");
  std::vector<const pt::ptree*> instance_data;
  for (const auto& a : data_pt)
    instance_data.push_back(&a.second);

  auto ptree_sorting = [](const pt::ptree* a, const pt::ptree* b) {
    const auto& a_name = a->get<std::string>("name");
    const auto& b_name = b->get<std::string>("name");
    const auto val = a_name.compare(b_name);
    if (val <= 0)
      return true;

    return false;
  };
  std::sort(instance_data.begin(), instance_data.end(), ptree_sorting);
  // Parsing the process info data
  pt::ptree status_pt;
  for (const auto& item : instance_data) {
    // Remove '_' from names and convert to PascalCase
    // IE Test_world => TestWorld
    std::string name = item->get<std::string>("name");
    if (!name.empty())
      name[0] = to_uppercase(name[0]);
    for (auto loc = name.find("_"); loc != std::string::npos; loc = name.find("_", loc)) {
      name.erase(loc, 1);
      // Capitalize the next letter if it exists
      if (loc < name.size())
        name[loc] = to_uppercase(name[loc]);
    }

    // Create the new data tree
    pt::ptree data_node_pt;
    data_node_pt.put("name", name);
    data_node_pt.put("value", item->get<std::string>("value"));
    status_pt.push_back(std::make_pair("", data_node_pt));
  }
  parsed_pt.add_child("process_info", status_pt);

  return parsed_pt;
}

pt::ptree
get_ps_instance_data(const xrt_core::device* device)
{
  pt::ptree all_instance_data;
  get_all_instance_data(device, all_instance_data);

  // Parse the returned data
  // First sort all of the instances
  const auto instance_list = get_sorted_instance_list(all_instance_data);

  // Parse each instance into trees based on their kernel names
  pt::ptree sorted_instance_tree;
  for (const auto& ps_instance_ptr : instance_list) {
    const auto& kernel_name = ps_instance_ptr->get<std::string>("kernel");

    // Parse the current instance into the approved schema
    const pt::ptree parsed_pt = parse_instance(*ps_instance_ptr);

    // Make sure there is a seperate tree for each kernel
    pt::ptree empty_tree;
    auto& kernel_tree = sorted_instance_tree.get_child(kernel_name, empty_tree);
    // Check if the tree is empty before adding the current instance
    const auto is_tree_empty = kernel_tree.empty();
    kernel_tree.push_back(std::make_pair("", parsed_pt));
    if (is_tree_empty)
      sorted_instance_tree.add_child(kernel_name, kernel_tree);
  }

  // Format the data into the controlled schema
  pt::ptree parsed_kernel_data;

  // Parse OS image data
  pt::ptree apu_image_pt;
  apu_image_pt.put("sysname", all_instance_data.get<std::string>("os.sysname"));
  apu_image_pt.put("release", all_instance_data.get<std::string>("os.release"));
  apu_image_pt.put("version", all_instance_data.get<std::string>("os.version"));
  apu_image_pt.put("machine", all_instance_data.get<std::string>("os.machine"));
  apu_image_pt.put("distribution", all_instance_data.get<std::string>("os.distribution"));
  apu_image_pt.put("model", all_instance_data.get<std::string>("os.model"));
  apu_image_pt.put("cores", all_instance_data.get<std::string>("os.cores"));
  // The received data arrives with a kB identifier for memory info
  // Replace it with a K for further processing
  std::string mem_total_data = std::regex_replace(all_instance_data.get<std::string>("os.mem_total"), std::regex("kB"), "K");
  apu_image_pt.put("mem_total", std::to_string(XBU::string_to_base_units(mem_total_data, XBU::unit::bytes)).append(" B"));
  std::string mem_available_data = std::regex_replace(all_instance_data.get<std::string>("os.mem_available"), std::regex("kB"), "K");
  apu_image_pt.put("mem_available", std::to_string(XBU::string_to_base_units(mem_available_data, XBU::unit::bytes)).append(" B"));
  std::string mem_free_data = std::regex_replace(all_instance_data.get<std::string>("os.mem_free"), std::regex("kB"), "K");
  apu_image_pt.put("mem_free", std::to_string(XBU::string_to_base_units(mem_free_data, XBU::unit::bytes)).append(" B"));
  uint64_t addr_data = std::stoull(all_instance_data.get<std::string>("os.address_space"), nullptr, 16);
  std::string addr_data_str = std::to_string(addr_data) + " B";
  apu_image_pt.put("address_space", addr_data_str);
  parsed_kernel_data.add_child("apu_image", apu_image_pt);

  parsed_kernel_data.add_child("ps_kernel_instances", sorted_instance_tree);
  return parsed_kernel_data;
}
