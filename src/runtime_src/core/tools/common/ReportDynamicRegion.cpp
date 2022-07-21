/**
 * Copyright (C) 2021, 2022 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include <boost/algorithm/string.hpp>
#include "ReportDynamicRegion.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include "core/common/utils.h"

#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>

namespace qr = xrt_core::query;

static void
test(const xrt_core::device * _pDevice, boost::property_tree::ptree &pt)
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

void
ReportDynamicRegion::getPropertyTreeInternal(const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportDynamicRegion::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  xrt::device device(_pDevice->get_device_id());
  std::stringstream ss;

  // Get dynamic region data
  ss << device.get_info<xrt::info::device::dynamic_regions>();
  boost::property_tree::read_json(ss, _pt);

  // Get APU PS kernel instance data
  // Perhaps this should be a type of device get_info?
  // Or be handled within dynamic regions?
  boost::property_tree::ptree ps_instance_data;
  test(_pDevice, ps_instance_data);

  // Parse the data into the existing tree
  boost::property_tree::ptree& pt_dfx = _pt.get_child("dynamic_regions");
  for (auto& k_dfx : pt_dfx) {
    boost::property_tree::ptree& cu_pt = k_dfx.second.get_child("compute_units");

    // Parse the compute units
    for (auto& cu_pair : cu_pt) {
      boost::property_tree::ptree& cu = cu_pair.second;
      if(cu.get<std::string>("type").compare("PS") != 0)
        continue;

      for (const auto& ps_instance : ps_instance_data) {
        const auto& ps_ptree = ps_instance.second;
        std::string kernel_name = ps_ptree.get<std::string>("ps_instance_meta.Kernel name");
        std::string instance_name = ps_ptree.get<std::string>("ps_instance_meta.Instance(CU) name");
        std::string fullname = kernel_name + ":" + instance_name;
        if (boost::equal(fullname, cu.get<std::string>("name")))
          cu.add_child("instance_data", ps_ptree);
      }
    }
  }
}

void 
ReportDynamicRegion::writeReport( const xrt_core::device* /*_pDevice*/,
                       const boost::property_tree::ptree& _pt, 
                       const std::vector<std::string>& /*_elementsFilter*/,
                       std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;
  boost::format cuFmt("    %-8s%-50s%-16s%-8s%-8s\n");

  //check if a valid CU report is generated
  const boost::property_tree::ptree& pt_dfx = _pt.get_child("dynamic_regions", empty_ptree);
  if(pt_dfx.empty())
    return;

  for(auto& k_dfx : pt_dfx) {
    const boost::property_tree::ptree& dfx = k_dfx.second;
    _output << "Xclbin UUID" << std::endl;
    _output << "  " + dfx.get<std::string>("xclbin_uuid", "N/A") << std::endl;
    _output << std::endl;

    const boost::property_tree::ptree& pt_cu = dfx.get_child("compute_units", empty_ptree);
    _output << "Compute Units" << std::endl;
    _output << "  PL Compute Units" << std::endl;
    _output << cuFmt % "Index" % "Name" % "Base_Address" % "Usage" % "Status";
    try {
      int index = 0;
      for(auto& kv : pt_cu) {
        const boost::property_tree::ptree& cu = kv.second;
        if(cu.get<std::string>("type").compare("PL") != 0)
          continue;
        std::string cu_status = cu.get_child("status").get<std::string>("bit_mask");
        uint32_t status_val = std::stoul(cu_status, nullptr, 16);
        _output << cuFmt % index++ %
          cu.get<std::string>("name") % cu.get<std::string>("base_address") %
          cu.get<std::string>("usage") % xrt_core::utils::parse_cu_status(status_val);
      }
    }
    catch( std::exception const& e) {
      _output << "ERROR: " <<  e.what() << std::endl;
    }
    _output << std::endl;

    //PS kernel report
    _output << "  PS Compute Units" << std::endl;
    boost::format ps_cu_fmt("    %-8s%-50s%-8s%-8s%-16s%-16s\n");
    _output << ps_cu_fmt % "Index" % "Name" % "Usage" % "Status" % "Address Space" % "Space in Use";
    try {
      int index = 0;
      for(auto& kv : pt_cu) {
        const boost::property_tree::ptree& cu = kv.second;
        if(cu.get<std::string>("type").compare("PS") != 0)
          continue;
        std::string cu_status = cu.get_child("status").get<std::string>("bit_mask");
        uint32_t status_val = std::stoul(cu_status, nullptr, 16);
        const auto& process_status = cu.get_child("instance_data.process_status");
        auto size_search = [](const std::pair<std::string, boost::property_tree::ptree>& node){ return boost::equal(node.second.get<std::string>("name"), std::string("VmSize"));};
        const auto& vm_size = std::find_if(process_status.begin(), process_status.end(), size_search);
        auto rss_search = [](const std::pair<std::string, boost::property_tree::ptree>& node){ return boost::equal(node.second.get<std::string>("name"), std::string("VmRSS"));};
        const auto& vm_rss = std::find_if(process_status.begin(), process_status.end(), rss_search);
        _output << ps_cu_fmt % index++ % cu.get<std::string>("name") % cu.get<std::string>("usage")
        % status_val % (*vm_size).second.get<std::string>("value") % (*vm_rss).second.get<std::string>("value");
      }
    }
    catch( std::exception const& e) {
      _output << "ERROR: " <<  e.what() << std::endl;
    }
  }

  _output << std::endl;
}
