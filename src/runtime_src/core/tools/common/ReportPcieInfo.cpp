/**
 * Copyright (C) 2021 Xilinx, Inc
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
#include "ReportPcieInfo.h"
#include "core/common/device.h"

// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>

void
ReportPcieInfo::getPropertyTreeInternal( const xrt_core::device * dev, 
                                              boost::property_tree::ptree &pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void 
ReportPcieInfo::getPropertyTree20202( const xrt_core::device * dev, 
                                           boost::property_tree::ptree &pt) const
{
  xrt::device device(dev->get_device_id());
  boost::property_tree::ptree pt_pcie_info;
  std::stringstream ss;
  ss << device.get_info<xrt::info::device::pcie_info>();
  boost::property_tree::read_json(ss, pt_pcie_info);
  
  // There can only be 1 root node
    pt.add_child("pcie_info", pt_pcie_info);
}

void 
ReportPcieInfo::writeReport( const xrt_core::device* /*_pDevice*/,
                             const boost::property_tree::ptree& _pt, 
                             const std::vector<std::string>& /*_elementsFilter*/,
                             std::ostream & _output) const
{
  _output << "Pcie Info\n";
  auto& pt_pcie = _pt.get_child("pcie_info");
  if(pt_pcie.empty()) {
    _output << "  Information unavailable" << std::endl; 
    return;
  }
  _output << boost::format("  %-22s : %s\n") % "Vendor" % pt_pcie.get<std::string>("vendor");
  _output << boost::format("  %-22s : %s\n") % "Device" % pt_pcie.get<std::string>("device");
  _output << boost::format("  %-22s : %s\n") % "Sub Device" % pt_pcie.get<std::string>("sub_device");
  _output << boost::format("  %-22s : %s\n") % "Sub Vendor" % pt_pcie.get<std::string>("sub_vendor");
  _output << boost::format("  %-22s : Gen%sx%s\n") % "PCIe" % pt_pcie.get<std::string>("link_speed_gbit_sec") % pt_pcie.get<std::string>("express_lane_width_count");
  _output << boost::format("  %-22s : %s\n") % "DMA Thread Count" % pt_pcie.get<std::string>("dma_thread_count", "0");
  _output << boost::format("  %-22s : %s\n") % "CPU Affinity" % pt_pcie.get<std::string>("cpu_affinity", "0");
  _output << boost::format("  %-22s : %s\n") % "Shared Host Memory" % pt_pcie.get<std::string>("shared_host_mem_size_bytes", "0");
  _output << boost::format("  %-22s : %s\n") % "Max Shared Host Memory" % pt_pcie.get<std::string>("max_shared_host_mem_aperture_bytes", "0");
  _output << boost::format("  %-22s : %s\n") % "Enabled Host Memory" % pt_pcie.get<std::string>("enabled_host_mem_size_bytes", "0");
  _output << std::endl;
}
