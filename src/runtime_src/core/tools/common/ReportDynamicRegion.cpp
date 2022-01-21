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
#include <boost/algorithm/string.hpp>
#include "ReportDynamicRegion.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include "core/common/utils.h"

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
    _output << cuFmt % "Index" % "Name" % "Base_Address" % "Usage" % "Status";
    try {
      int index = 0;
      for(auto& kv : pt_cu) {
        const boost::property_tree::ptree& cu = kv.second;
        if(cu.get<std::string>("type").compare("PS") != 0)
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
  }

  _output << std::endl;
}
