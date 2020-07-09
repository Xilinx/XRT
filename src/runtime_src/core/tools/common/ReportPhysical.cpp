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
populate_temp(const xrt_core::device * device, const std::string loc_id, const std::string desc)
{
  boost::property_tree::ptree pt;
  uint64_t temp;
  try {
    temp = xrt_core::device_query<QueryRequestType>(device);
  } catch (const std::exception ex){
    pt.put("error_msg", ex.what());
  }
  
  pt.put("location_id", loc_id);
  pt.put("description",desc);
  pt.put("temp_C", temp);
  pt.put("is_present", temp != 0 ? "true" : "false");
  
  return pt;
}

template <typename QRVoltage, typename QRCurrent>
boost::property_tree::ptree
populate_sensor(const xrt_core::device * device, const std::string loc_id, const std::string desc)
{
  boost::property_tree::ptree pt;
  pt.put("id", loc_id);
  pt.put("description", desc);

  uint64_t voltage = 0, current = 0;
  try {
    if (!std::is_same<QRVoltage, qr::noop>::value)
      voltage = xrt_core::device_query<QRVoltage>(device);
  } catch (const std::exception ex){
    pt.put("voltage.error_msg", ex.what());
  }
  pt.put("voltage.volts", QRVoltage::to_string(voltage));
  pt.put("voltage.is_present", voltage != 0 ? "true" : "false");

  try {
    if (!std::is_same<QRCurrent, qr::noop>::value)
      current = xrt_core::device_query<QRCurrent>(device);
  } catch (const std::exception ex){
    pt.put("current.error_msg", ex.what());
  }
  pt.put("current.amps", QRCurrent::to_string(current));
  pt.put("current.is_present", current != 0 ? "true" : "false");
  
  return pt;
}

boost::property_tree::ptree
populate_fan(const xrt_core::device * device, const std::string loc_id, const std::string desc)
{
  boost::property_tree::ptree pt;
  uint64_t temp, rpm;
  std::string is_present;
  try {
    temp = xrt_core::device_query<qr::fan_trigger_critical_temp>(device);
    rpm = xrt_core::device_query<qr::fan_speed_rpm>(device);
    is_present = xrt_core::device_query<qr::fan_fan_presence>(device);
  } catch (const std::exception ex){
    pt.put("error_msg", ex.what());
  }
  
  pt.put("location_id", loc_id);
  pt.put("description", desc);
  pt.put("critical_trigger_temp_C", temp);
  pt.put("speed_rpm", rpm);
  pt.put("is_present", qr::fan_fan_presence::to_string(is_present));
  
  return pt;
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
  boost::property_tree::ptree pt;
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
  pt.add_child("thermals", thermal_array);


  //--- fan ----------
  boost::property_tree::ptree fan_array;
  fan_array.push_back(std::make_pair("", populate_fan(_pDevice, "fpga_fan_1", "FPGA Fan 1")));
  pt.add_child("fans", fan_array);


  //--- electrical --------------
  boost::property_tree::ptree sensor_array;
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v12v_aux_millivolts, qr::v12v_aux_milliamps>(_pDevice, "12v_aux", "12 Volts Auxillary")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v12v_pex_millivolts, qr::v12v_pex_milliamps>(_pDevice, "12v_pex", "12 Volts Pex")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v3v3_pex_millivolts, qr::v3v3_pex_milliamps>(_pDevice, "3v3_pex", "3v3 Pex")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v3v3_aux_millivolts, qr::v3v3_aux_milliamps>(_pDevice, "3v3_aux", "3v3 Auxillary")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::int_vcc_millivolts, qr::int_vcc_milliamps>(_pDevice, "vccint", "Vcc Int")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::int_vcc_io_millivolts, qr::int_vcc_io_milliamps>(_pDevice, "vccint_io", "Vcc Int IO")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::ddr_vpp_bottom_millivolts, qr::noop>(_pDevice, "ddr_vpp_btm", "DDR vpp Bottom")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::ddr_vpp_top_millivolts, qr::noop>(_pDevice, "ddr_vpp_top", "DDR vpp Top")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v5v5_system_millivolts, qr::noop>(_pDevice, "5v5_system", "5v5 System")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v1v2_vcc_top_millivolts, qr::noop>(_pDevice, "1v2_top", "1v2 Top")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v1v2_vcc_bottom_millivolts, qr::noop>(_pDevice, "vcc_1v2_btm", "Vcc 1v2 Bottom")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v0v9_vcc_millivolts, qr::noop>(_pDevice, "0v9_vcc", "0v9 Vcc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v12v_sw_millivolts, qr::noop>(_pDevice, "12v_sw", "12 Volts SW")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::mgt_vtt_millivolts, qr::noop>(_pDevice, "mgt_vtt", "Mgt Vtt")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v3v3_vcc_millivolts, qr::noop>(_pDevice, "3v3_vcc", "3v3 Vcc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::hbm_1v2_millivolts, qr::noop>(_pDevice, "hbm_1v2", "HBM 1v2")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v2v5_vpp_millivolts, qr::noop>(_pDevice, "vpp2v5", "Vpp 2v5")));
  
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::_12v_aux1_millivolts, qr::noop>(_pDevice, "12v_aux1", "12v Aux1")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::noop, qr::vcc1v2_i_milliamps>(_pDevice, "vcc1v2_i", "Vcc1v2 i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::noop, qr::v12_in_i_milliamps>(_pDevice, "v12_in_i", "V12 in i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::noop, qr::v12_in_aux0_i_milliamps>(_pDevice, "v12_in_aux0_i", "V12 in Aux0 i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::noop, qr::v12_in_aux1_i_milliamps>(_pDevice, "v12_in_aux1_i", "V12 in Aux1 i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::vcc_aux_millivolts, qr::noop>(_pDevice, "vcc_aux", "Vcc Aux")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::vcc_aux_pmc_millivolts, qr::noop>(_pDevice, "vcc_aux_pmc", "Vcc Aux Pmc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::vcc_ram_millivolts, qr::noop>(_pDevice, "vcc_ram", "Vcc Ram")));
  
  pt.add_child("electricals.power_rails", sensor_array);


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

  _output << "Thermals\n";
  boost::property_tree::ptree& thermals = _pt.get_child("physical.thermals", empty_ptree);
  for(auto& kv : thermals) {
    boost::property_tree::ptree& pt_temp = kv.second;
    if(!pt_temp.get<bool>("is_present", false))
      continue;
    _output << boost::format("  %-20s : %s C\n") % pt_temp.get<std::string>("description") % pt_temp.get<std::string>("temp_C");
  }
  _output << std::endl;

  _output << "Fans\n";
  boost::property_tree::ptree& fans = _pt.get_child("physical.fans", empty_ptree);
  for(auto& kv : fans) {
    boost::property_tree::ptree& pt_fan = kv.second;
    if(!pt_fan.get<bool>("is_present", false))
      continue;
    _output << boost::format("  %-10s\n") % pt_fan.get<std::string>("description");
    _output << boost::format("    %-22s: %s C\n") % "Critical Trigger Temp" % pt_fan.get<std::string>("critical_trigger_temp_C");
    _output << boost::format("    %-22s: %s RPM\n") % "Speed" % pt_fan.get<std::string>("speed_rpm");
  }
  _output << std::endl;

  _output << "Electricals\n";
  boost::property_tree::ptree& electricals = _pt.get_child("physical.electricals.power_rails", empty_ptree);
  for(auto& kv : electricals) {
    boost::property_tree::ptree& pt_sensor = kv.second;
    if(pt_sensor.get<bool>("voltage.is_present") || pt_sensor.get<bool>("current.is_present"))
      _output << boost::format("  %-10s\n") % pt_sensor.get<std::string>("description");
    if(pt_sensor.get<bool>("voltage.is_present"))
      _output << boost::format("    %-10s: %s V\n") % "Voltage" % pt_sensor.get<std::string>("voltage.volts");
    if(pt_sensor.get<bool>("current.is_present"))
      _output << boost::format("    %-10s: %s A\n") % "Current" % pt_sensor.get<std::string>("current.amps");
  }
  _output << std::endl;
  
}
