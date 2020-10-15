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
#include "ReportThermal.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
namespace qr = xrt_core::query;

template <typename QueryRequestType>
boost::property_tree::ptree
populate_temp(const xrt_core::device * device, const std::string& loc_id, const std::string& desc)
{
  boost::property_tree::ptree pt;
  uint64_t temp;
  try {
    temp = xrt_core::device_query<QueryRequestType>(device);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }
  
  pt.put("location_id", loc_id);
  pt.put("description", desc);
  pt.put("temp_C", temp);
  pt.put("is_present", temp != 0 ? "true" : "false");
  
  return pt;
}

void
ReportThermal::getPropertyTreeInternal( const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportThermal::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree thermal_array;
  //--- pcb ----------
  thermal_array.push_back(std::make_pair("", populate_temp<qr::temp_card_top_front>(_pDevice, "pcb_top_front", "PCB Top Front")));
  thermal_array.push_back(std::make_pair("", populate_temp<qr::temp_card_top_rear>(_pDevice, "pcb_top_rear", "PCB Top Rear")));
  thermal_array.push_back(std::make_pair("", populate_temp<qr::temp_card_bottom_front>(_pDevice, "pcb_bottom_front", "PCB Bottom Front")));

  //--- cage ----------
  thermal_array.push_back(std::make_pair("", populate_temp<qr::cage_temp_0>(_pDevice, "cage_temp_0", "Cage0")));
  thermal_array.push_back(std::make_pair("", populate_temp<qr::cage_temp_1>(_pDevice, "cage_temp_1", "Cage1")));
  thermal_array.push_back(std::make_pair("", populate_temp<qr::cage_temp_2>(_pDevice, "cage_temp_2", "Cage2")));
  thermal_array.push_back(std::make_pair("", populate_temp<qr::cage_temp_3>(_pDevice, "cage_temp_3", "Cage3")));

  // --- fpga, vccint, hbm -------------
  thermal_array.push_back(std::make_pair("", populate_temp<qr::temp_fpga>(_pDevice, "fpga0", "FPGA")));
  thermal_array.push_back(std::make_pair("", populate_temp<qr::int_vcc_temp>(_pDevice, "int_vcc", "Int Vcc")));
  thermal_array.push_back(std::make_pair("", populate_temp<qr::hbm_temp>(_pDevice, "fpga_hbm", "FPGA HBM")));


  // There can only be 1 root node
  _pt.add_child("thermals", thermal_array);
}

void 
ReportThermal::writeReport( const xrt_core::device * _pDevice,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  bool thermals_present = false;
  _output << "Thermals\n";
  boost::property_tree::ptree& thermals = _pt.get_child("thermals", empty_ptree);
  for(auto& kv : thermals) {
    boost::property_tree::ptree& pt_temp = kv.second;
    if(!pt_temp.get<bool>("is_present", false))
      continue;
    thermals_present = true;
    _output << boost::format("  %-20s : %s C\n") % pt_temp.get<std::string>("description") % pt_temp.get<std::string>("temp_C");
  }

  if(!thermals_present) {
    _output << "  No temperature sensors are present" << std::endl;
  }
  _output << std::endl;
  
}
