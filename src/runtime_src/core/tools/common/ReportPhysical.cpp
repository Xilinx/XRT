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
#include "ReportPhysical.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
namespace qr = xrt_core::query;

template <typename QueryRequestType>
boost::property_tree::ptree
get_temp(const xrt_core::device * device)
{
  boost::property_tree::ptree _pt;
  auto val = xrt_core::device_query<QueryRequestType>(device);
  _pt.put("name", QueryRequestType::name());
  _pt.put("value", QueryRequestType::to_string(val));
  
  return _pt;
}

template <typename QRVoltage, typename QRCurrent>
boost::property_tree::ptree
get_sensor(const xrt_core::device * device, std::string sensor_name)
{
  boost::property_tree::ptree _pt;
  _pt.put("name", sensor_name);
  auto vol = xrt_core::device_query<QRVoltage>(device);
  _pt.put(QRVoltage::name(), vol);
  auto curr = xrt_core::device_query<QRCurrent>(device);
  _pt.put(QRCurrent::name(), curr);
  _pt.put("is_present", vol != 0 || curr != 0);
  
  return _pt;
}

std::pair<std::string, std::string>
parse_name_and_units(std::string _name)
{
    std::vector<std::string> units = {"C", "rpm"};
    std::string unit = _name.substr(_name.rfind("_") + 1);
    if(std::find(units.begin(),units.end(),unit) != units.end()) {
        _name.erase(_name.rfind("_"));
    } else {
        unit = "";
    }
    std::replace( _name.begin(), _name.end(), '_', ' ');
    return std::make_pair(_name, unit);
}

void
ReportPhysical::getPropertyTreeInternal( const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20201 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20201(_pDevice, _pt);
}

void 
ReportPhysical::getPropertyTree20201( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  // using the existing function in device.cpp
  // xrt_core::ptree_updater<xrt_core::query::temp_card_top_front>::query_and_put(_pDevice, pt_thermal);
  boost::property_tree::ptree pt;
  boost::property_tree::ptree thermal_array;
  //--- pcb ----------
  thermal_array.push_back(std::make_pair("", get_temp<qr::temp_card_top_front>(_pDevice)));
  thermal_array.push_back(std::make_pair("", get_temp<qr::temp_card_top_rear>(_pDevice)));
  thermal_array.push_back(std::make_pair("", get_temp<qr::temp_card_bottom_front>(_pDevice)));
  
  //--- fan ----------
  thermal_array.push_back(std::make_pair("", get_temp<qr::fan_trigger_critical_temp>(_pDevice)));
  thermal_array.push_back(std::make_pair("", get_temp<qr::fan_fan_presence>(_pDevice)));
  thermal_array.push_back(std::make_pair("", get_temp<qr::fan_speed_rpm>(_pDevice)));

  //--- cage ----------
  thermal_array.push_back(std::make_pair("", get_temp<qr::cage_temp_0>(_pDevice)));
  thermal_array.push_back(std::make_pair("", get_temp<qr::cage_temp_1>(_pDevice)));
  thermal_array.push_back(std::make_pair("", get_temp<qr::cage_temp_2>(_pDevice)));
  thermal_array.push_back(std::make_pair("", get_temp<qr::cage_temp_3>(_pDevice)));

  // --- fpga, vccint, hbm -------------
  thermal_array.push_back(std::make_pair("", get_temp<qr::temp_fpga>(_pDevice)));
  thermal_array.push_back(std::make_pair("", get_temp<qr::int_vcc_temp>(_pDevice)));
  thermal_array.push_back(std::make_pair("", get_temp<qr::hbm_temp>(_pDevice)));
  pt.add_child("thermal", thermal_array);

  //--- electrical --------------
  boost::property_tree::ptree sensor_array;
  boost::property_tree::ptree pt_12v_aux = get_sensor<qr::v12v_aux_millivolts, qr::v12v_aux_milliamps>(_pDevice, "12V AUX");
  boost::property_tree::ptree pt_12v_pex = get_sensor<qr::v12v_pex_millivolts, qr::v12v_pex_milliamps>(_pDevice, "12V PEX");
  sensor_array.push_back(std::make_pair("", pt_12v_aux));
  sensor_array.push_back(std::make_pair("", pt_12v_pex));
  pt.add_child("electrical", sensor_array);

  // There can only be 1 root node
  _pt.add_child("physical", pt);
}

void 
ReportPhysical::writeReport( const xrt_core::device * _pDevice,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << "Temperature\n";
  boost::property_tree::ptree& thermals = _pt.get_child("physical.thermal", empty_ptree);
  for(auto& kv : thermals) {
    boost::property_tree::ptree& temp = kv.second;
    auto x = parse_name_and_units(temp.get<std::string>("name"));
    _output << boost::format("  %-20s : %s %s\n") % x.first % temp.get<std::string>("value") % x.second;
  }
    
    _output << std::endl;
    _output << "Electrical\n";
    boost::property_tree::ptree& electricals = _pt.get_child("physical.electrical", empty_ptree);
    for(auto& kv : electricals) {
      boost::property_tree::ptree& sensor = kv.second;
      if(sensor.get<bool>("is_present")) {
        _output << boost::format("  %-10s\n") % sensor.get<std::string>("name");
        _output << boost::format("    %-10s: %s mV\n") % "Voltage" % sensor.get<std::string>("voltage_mV");
        _output << boost::format("    %-10s: %s mA\n") % "Current" % sensor.get<std::string>("current_mA");
      } 
    }
    _output << std::endl;
}
