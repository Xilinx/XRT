/**
 * Copyright (C) 2020 Xilinx, Inc
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
#include "ReportCu.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include "core/common/utils.h"

namespace qr = xrt_core::query;

boost::property_tree::ptree
get_cu_status(uint32_t cu_status)
{
  boost::property_tree::ptree pt;
  std::vector<std::string> bit_set;

  if (cu_status & 0x1)
    bit_set.push_back("START");
  if (cu_status & 0x2)
    bit_set.push_back("DONE");
  if (cu_status & 0x4)
    bit_set.push_back("IDLE");
  if (cu_status & 0x8)
    bit_set.push_back("READY");
  if (cu_status & 0x10)
    bit_set.push_back("RESTART");

  pt.put("bit_mask",	boost::str(boost::format("0x%x") % cu_status));
  boost::property_tree::ptree ptSt_arr;
  for(auto& str : bit_set)
    ptSt_arr.push_back(std::make_pair("", boost::property_tree::ptree(str)));

  if (!ptSt_arr.empty())
    pt.add_child( std::string("bits_set"), ptSt_arr);

  return pt;
}

boost::property_tree::ptree
populate_cus(const xrt_core::device *device)
{
  boost::property_tree::ptree pt;
  std::vector<char> ip_buf;
  std::vector<std::tuple<uint64_t, uint32_t, uint32_t>> cu_stats; // tuple <base_addr, usage, status>

  try {
    ip_buf = xrt_core::device_query<qr::ip_layout_raw>(device);
    cu_stats = xrt_core::device_query<qr::kds_cu_info>(device);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
    return pt;
  }

  if(ip_buf.empty() || cu_stats.empty()) {
    return pt;
  }

  const ip_layout *layout = reinterpret_cast<const ip_layout*>(ip_buf.data());
  for (int i = 0; i < layout->m_count; i++) {
    if (layout->m_ip_data[i].m_type != IP_KERNEL)
      continue;

    for(auto& stat : cu_stats) {
      uint64_t base_addr = std::get<0>(stat);
      if (layout->m_ip_data[i].m_base_address == base_addr) {
        uint32_t usage = std::get<1>(stat);
        uint32_t status = std::get<2>(stat);

        boost::property_tree::ptree ptCu;
        ptCu.put( "name",			layout->m_ip_data[i].m_name);
        ptCu.put( "base_address",		boost::str(boost::format("0x%x") % base_addr));
        ptCu.put( "usage",			usage);
        ptCu.add_child( std::string("status"),	get_cu_status(status));
        pt.push_back(std::make_pair("", ptCu));
      }
    }
  }

  return pt;
}

void
ReportCu::getPropertyTreeInternal(const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportCu::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  // There can only be 1 root node
  _pt.add_child("compute_units", populate_cus(_pDevice));
}

void 
ReportCu::writeReport( const xrt_core::device * _pDevice,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  //check if a valid report is generated
  boost::property_tree::ptree& v = _pt.get_child("compute_units");
  if(v.empty())
    return;

  _output << "Compute Units" << std::endl;
  boost::format cuFmt("%-8s%-24s%-16s%-8s%-8s\n");
  _output << cuFmt % "Index" % "Name" % "Base_Address" % "Usage" % "Status";
  try {
    int index = 0;
    for(auto& kv : v) {
      boost::property_tree::ptree& cu = kv.second;
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
}
