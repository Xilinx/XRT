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
#include "ReportCu.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include "core/common/utils.h"

namespace qr = xrt_core::query;

boost::property_tree::ptree
populate_cus(const xrt_core::device *device, const std::string& desc)
{
  boost::property_tree::ptree pt;
  std::vector<char> ip_buf;
  std::vector<std::tuple<int, int, int>> cu_stats;

  pt.put("description", desc);

  try {
    ip_buf = xrt_core::device_query<qr::ip_layout_raw>(device);
    cu_stats = xrt_core::device_query<qr::kds_cu_info>(device);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }

  if(ip_buf.empty() || cu_stats.empty())
    return pt;

  const ip_layout *layout = (ip_layout *)ip_buf.data();
  boost::property_tree::ptree ptCu_array;
  for (int i = 0; i < layout->m_count; i++) {
    if (layout->m_ip_data[i].m_type != IP_KERNEL)
      continue;

    for(auto& stat : cu_stats) {
      if (layout->m_ip_data[i].m_base_address == (uint64_t)std::get<0>(stat)) {
        uint32_t usage = std::get<1>(stat);
        uint32_t status = std::get<2>(stat);

        boost::property_tree::ptree ptCu;
        ptCu.put( "name",         layout->m_ip_data[i].m_name);
        ptCu.put( "base_address", boost::str(boost::format("0x%x") % std::get<0>(stat)));
        ptCu.put( "usage",        usage);
        ptCu.put( "status",       xrt_core::utils::parse_cu_status(status));
        ptCu_array.push_back(std::make_pair("", ptCu));
      }
    }
  }

  pt.add_child( std::string("compute_units"), ptCu_array);

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
  _pt.add_child("compute_unit", populate_cus(_pDevice, "Compute Units Information"));
}

void 
ReportCu::writeReport( const xrt_core::device * _pDevice,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << _pt.get<std::string>("compute_unit.description") << std::endl;
  _output << boost::format("    %-8s%-24s%-16s%-8s%-8s\n") % "Index" % "Name" % "Base_Address" % "Usage" % "Status";
  try {
    int index = 0;
    boost::property_tree::ptree& v = _pt.get_child("compute_unit.compute_units");
    for(auto& kv : v) {
      boost::property_tree::ptree& cu = kv.second;
      _output << boost::format("    %-8s%-24s%-16s%-8s%-8s\n") % index++ %
	      cu.get<std::string>("name") % cu.get<std::string>("base_address") %
	      cu.get<std::string>("usage") % cu.get<std::string>("status");
    }
  }
  catch( std::exception const& e) {
    _output <<  e.what() << std::endl;
  }

  _output << std::endl;
}
