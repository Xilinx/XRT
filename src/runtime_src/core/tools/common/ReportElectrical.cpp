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
#include "ReportElectrical.h"
#include "XBUtilities.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
namespace qr = xrt_core::query;

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
  } catch (const std::exception& ex){
    pt.put("voltage.error_msg", ex.what());
  }
  pt.put("voltage.volts", XBUtilities::format_base10_shiftdown3(voltage));
  pt.put("voltage.is_present", voltage != 0 ? "true" : "false");

  try {
    if (!std::is_same<QRCurrent, qr::noop>::value)
      current = xrt_core::device_query<QRCurrent>(device);
  } catch (const std::exception& ex){
    pt.put("current.error_msg", ex.what());
  }
  pt.put("current.amps", XBUtilities::format_base10_shiftdown3(current));
  pt.put("current.is_present", current != 0 ? "true" : "false");
  
  return pt;
}

void
ReportElectrical::getPropertyTreeInternal( const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportElectrical::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  boost::property_tree::ptree pt;
  
  pt.put("power_consumption_watts", XBUtilities::format_base10_shiftdown6(xrt_core::device_query<qr::power_microwatts>(_pDevice)));
  boost::property_tree::ptree sensor_array;
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v12v_aux_millivolts, qr::v12v_aux_milliamps>(_pDevice, "12v_aux", "12 Volts Auxillary")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v12v_pex_millivolts, qr::v12v_pex_milliamps>(_pDevice, "12v_pex", "12 Volts PCI Express")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v3v3_pex_millivolts, qr::v3v3_pex_milliamps>(_pDevice, "3v3_pex", "3.3 Volts PCI Express")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v3v3_aux_millivolts, qr::v3v3_aux_milliamps>(_pDevice, "3v3_aux", "3.3 Volts Auxillary")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::int_vcc_millivolts, qr::int_vcc_milliamps>(_pDevice, "vccint", "Internal FPGA Vcc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::int_vcc_io_millivolts, qr::int_vcc_io_milliamps>(_pDevice, "vccint_io", "Internal FPGA Vcc IO")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::ddr_vpp_bottom_millivolts, qr::noop>(_pDevice, "ddr_vpp_btm", "DDR Vpp Bottom")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::ddr_vpp_top_millivolts, qr::noop>(_pDevice, "ddr_vpp_top", "DDR Vpp Top")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v5v5_system_millivolts, qr::noop>(_pDevice, "5v5_system", "5.5 Volts System")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v1v2_vcc_top_millivolts, qr::noop>(_pDevice, "1v2_top", "Vcc 1.2 Volts Top")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v1v2_vcc_bottom_millivolts, qr::noop>(_pDevice, "vcc_1v2_btm", "Vcc 1.2 Volts Bottom")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v0v9_vcc_millivolts, qr::noop>(_pDevice, "0v9_vcc", "0.9 Volts Vcc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v12v_sw_millivolts, qr::noop>(_pDevice, "12v_sw", "12 Volts SW")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::mgt_vtt_millivolts, qr::noop>(_pDevice, "mgt_vtt", "Mgt Vtt")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v3v3_vcc_millivolts, qr::noop>(_pDevice, "3v3_vcc", "3.3 Volts Vcc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::hbm_1v2_millivolts, qr::noop>(_pDevice, "hbm_1v2", "1.2 Volts HBM")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v2v5_vpp_millivolts, qr::noop>(_pDevice, "vpp2v5", "Vpp 2.5 Volts")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::v12_aux1_millivolts, qr::noop>(_pDevice, "12v_aux1", "12 Volts Aux1")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::noop, qr::vcc1v2_i_milliamps>(_pDevice, "vcc1v2_i", "Vcc 1.2 Volts i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::noop, qr::v12_in_i_milliamps>(_pDevice, "v12_in_i", "V12 in i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::noop, qr::v12_in_aux0_i_milliamps>(_pDevice, "v12_in_aux0_i", "V12 in Aux0 i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::noop, qr::v12_in_aux1_i_milliamps>(_pDevice, "v12_in_aux1_i", "V12 in Aux1 i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::vcc_aux_millivolts, qr::noop>(_pDevice, "vcc_aux", "Vcc Auxillary")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::vcc_aux_pmc_millivolts, qr::noop>(_pDevice, "vcc_aux_pmc", "Vcc Auxillary Pmc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<qr::vcc_ram_millivolts, qr::noop>(_pDevice, "vcc_ram", "Vcc Ram")));
  
  pt.add_child("power_rails", sensor_array);


  // There can only be 1 root node
  _pt.add_child("electrical", pt);
}

void 
ReportElectrical::writeReport( const xrt_core::device * _pDevice,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);

  _output << "Electrical\n";
  boost::property_tree::ptree& electricals = _pt.get_child("electrical.power_rails", empty_ptree);
  _output << boost::format("  %-22s: %s Watts\n\n") % "Power" % _pt.get<std::string>("electrical.power_consumption_watts");
  _output << boost::format("  %-22s: %6s   %6s\n") % "Power Rails" % "Voltage" % "Current";
  for(auto& kv : electricals) {
    boost::property_tree::ptree& pt_sensor = kv.second;
    std::string name = pt_sensor.get<std::string>("description");
    bool volts_is_present = pt_sensor.get<bool>("voltage.is_present");
    std::string volts = pt_sensor.get<std::string>("voltage.volts");
    bool amps_is_present = pt_sensor.get<bool>("current.is_present");
    std::string amps = pt_sensor.get<std::string>("current.amps");

    if(volts_is_present && amps_is_present)
      _output << boost::format("  %-22s: %6s V, %6s A\n") % name % volts % amps;
    else if(volts_is_present)
      _output << boost::format("  %-22s: %6s V\n") % name % volts;
    else if(amps_is_present)
      _output << boost::format("  %-22s: %16s A\n") % name % amps;
  }
  _output << std::endl;
  
}
