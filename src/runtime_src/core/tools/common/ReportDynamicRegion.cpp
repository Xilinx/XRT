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
  ss << device.get_info<xrt::info::device::dynamic_regions>();
  boost::property_tree::read_json(ss, _pt);
}

static const int COUNT = 4096;
struct process_header
{
    size_t count;
};
#define MAX_DATA_LENGTH 16
struct process_data
{
    char name[MAX_DATA_LENGTH];
    char vsz[MAX_DATA_LENGTH];
    char stat[MAX_DATA_LENGTH];
    char etime[MAX_DATA_LENGTH];
    char cpu[MAX_DATA_LENGTH];
    char cpu_util[MAX_DATA_LENGTH];

    friend std::ostream& operator<<(std::ostream& out, const struct process_data& d)
    {
        out << d.name << " " << d.etime << " " << d.vsz 
            << " " << d.stat << " " << d.cpu << " " << d.cpu_util;
        return out;
    }
};

std::map<std::string, struct process_data> test()
{
  // TODO this will be removed when the PS kernel is built in
  std::string b_file = "/ps_validate_bandwidth.xclbin";
  std::string binaryfile = "/opt/xilinx/firmware/vck5000/gen4x8-xdma/base/test" + b_file;

  auto device = xrt::device {"0000:17:00.1"};
  auto uuid = device.load_xclbin(binaryfile);
  auto hello_world = xrt::kernel(device, uuid.get(), "hello_world");
  const size_t DATA_SIZE = COUNT * sizeof(char);
  auto bo0 = xrt::bo(device, DATA_SIZE, hello_world.group_id(0));
  auto bo0_map = bo0.map<char*>();
  std::fill(bo0_map, bo0_map + COUNT, 0);

  bo0.sync(XCL_BO_SYNC_BO_TO_DEVICE, DATA_SIZE, 0);

  auto run = hello_world(bo0, COUNT);
  run.wait();

  //Get the output;
  bo0.sync(XCL_BO_SYNC_BO_FROM_DEVICE, DATA_SIZE, 0);
  struct process_header* header = (struct process_header*) bo0_map;
  std::cout << "Data Count: " << header->count << std::endl;

  // Create and populate the data map
  std::map<std::string, struct process_data> data_map;
  struct process_data* data = (struct process_data*) (bo0_map + sizeof(struct process_header));
  for (decltype(header->count) i = 0; i < header->count; i++)
    data_map.emplace(data[i].name, data[i]);
  return data_map;
}

void 
ReportDynamicRegion::writeReport( const xrt_core::device* /*_pDevice*/,
                       const boost::property_tree::ptree& _pt, 
                       const std::vector<std::string>& /*_elementsFilter*/,
                       std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;
  boost::format cuFmt("    %-8s%-50s%-16s%-8s%-8s\n");

  auto data = test();

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
    boost::format ps_cu_fmt("    %-8s%-50s%-8s%-8s%-8s%-16s%-8s%-8s\n");
    _output << ps_cu_fmt % "Index" % "Name" % "Usage" % "Status" % "VSZ" % "Elapsed Time" % "CPU" % "CPU%";
    try {
      int index = 0;
      for(auto& kv : pt_cu) {
        const boost::property_tree::ptree& cu = kv.second;
        if(cu.get<std::string>("type").compare("PS") != 0)
          continue;
        std::string cu_status = cu.get_child("status").get<std::string>("bit_mask");
        uint32_t status_val = std::stoul(cu_status, nullptr, 16);
        // Currently the name that is passed back does not match
        // Once the updates occur in SKD/APU the returned name should correlate
        std::string fullname = cu.get<std::string>("name");
        // Remove the name truncation once the update is ready
        std::string name = fullname.substr(fullname.find(":") + 1, MAX_DATA_LENGTH - 1);
        auto it = data.find(name);
        if (it != data.end()) {
          auto& process_info = it->second;
          _output << ps_cu_fmt % index++ %
            cu.get<std::string>("name") % cu.get<std::string>("usage") % xrt_core::utils::parse_cu_status(status_val) %
            process_info.vsz % process_info.etime % process_info.cpu % process_info.cpu_util;
        }
      }
    }
    catch( std::exception const& e) {
      _output << "ERROR: " <<  e.what() << std::endl;
    }
  }

  _output << std::endl;
}
