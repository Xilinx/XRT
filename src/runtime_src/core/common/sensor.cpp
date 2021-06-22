/**
 * Copyright (C) 2018-2021 Xilinx, Inc
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
#define XRT_CORE_COMMON_SOURCE

// Local - Include Files
#include "sensor.h"
#include "query_requests.h"
#include "utils.h"

// System - Include Files
#include <sstream>
#include <iomanip>

// ------ S T A T I C   F U N C T I O N S -------------------------------------
namespace {
/**
 * Saves voltage-current pair of a sensor into a boost::property_tree
 * Converts mV and mA into V and A before adding to the tree
 * 
 * @tparam QRVoltage voltage query for a sensor; query::noop if query DNE
 * @tparam QRCurrent current query for the same sensor; query::noop if query DNE
 * @param loc_id human readable sensor identifier
 * @desc description about the sensor
 */
template <typename QRVoltage, typename QRCurrent>
static boost::property_tree::ptree
populate_sensor(const xrt_core::device * device, const std::string& loc_id, const std::string& desc)
{
  boost::property_tree::ptree pt;
  pt.put("id", loc_id);
  pt.put("description", desc);

  uint64_t voltage = 0, current = 0;
  try {
    if (!std::is_same<QRVoltage, xrt_core::query::noop>::value)
      voltage = xrt_core::device_query<QRVoltage>(device);
  } catch (const std::exception& ex) {
    pt.put("voltage.error_msg", ex.what());
  }
  pt.put("voltage.volts", xrt_core::utils::format_base10_shiftdown3(voltage));
  pt.put("voltage.is_present", voltage != 0 ? "true" : "false");

  try {
    if (!std::is_same<QRCurrent, xrt_core::query::noop>::value)
      current = xrt_core::device_query<QRCurrent>(device);
  } catch (const std::exception& ex) {
    pt.put("current.error_msg", ex.what());
  }
  pt.put("current.amps", xrt_core::utils::format_base10_shiftdown3(current));
  pt.put("current.is_present", current != 0 ? "true" : "false");
  
  return pt;
}

template <typename QueryRequestType>
static boost::property_tree::ptree
populate_temp(const xrt_core::device * device, const std::string& loc_id, const std::string& desc)
{
  boost::property_tree::ptree pt;
  uint64_t temp = 0;
  try {
    temp = xrt_core::device_query<QueryRequestType>(device);
  } catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
  }
  
  pt.put("location_id", loc_id);
  pt.put("description", desc);
  pt.put("temp_C", temp);
  pt.put("is_present", temp != 0 ? "true" : "false");
  
  return pt;
}

/**
 * device query returns a level which 
 * need to be converted to human readable power in watts
 * 0 -> 75W
 * 1 -> 150W
 * 2 -> 225W
 */
static std::string
lvl_to_power_watts(uint64_t lvl)
{
  constexpr std::array<const char*, 3> powers{ "75", "150", "225" };
  return lvl < powers.size() ? powers[lvl] : "N/A";
}

static boost::property_tree::ptree
populate_fan(const xrt_core::device * device, const std::string& loc_id, const std::string& desc)
{
  boost::property_tree::ptree pt;
  uint64_t temp = 0, rpm = 0;
  std::string is_present;
  try {
    temp = xrt_core::device_query<xrt_core::query::fan_trigger_critical_temp>(device);
    rpm = xrt_core::device_query<xrt_core::query::fan_speed_rpm>(device);
    is_present = xrt_core::device_query<xrt_core::query::fan_fan_presence>(device);
  } catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
  }
  
  pt.put("location_id", loc_id);
  pt.put("description", desc);
  pt.put("critical_trigger_temp_C", temp);
  pt.put("speed_rpm", rpm);
  pt.put("is_present", xrt_core::query::fan_fan_presence::to_string(is_present));
  
  return pt;
}

} //unnamed namespace

namespace xrt_core {
namespace sensor {

boost::property_tree::ptree
read_power_rails(const xrt_core::device * device)
{
  boost::property_tree::ptree root;
  boost::property_tree::ptree sensor_array;
  sensor_array.push_back(std::make_pair("", 
      populate_sensor<xrt_core::query::v12v_aux_millivolts, xrt_core::query::v12v_aux_milliamps>(device, "12v_aux", "12 Volts Auxillary")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v12v_pex_millivolts, xrt_core::query::v12v_pex_milliamps>(device, "12v_pex", "12 Volts PCI Express")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v3v3_pex_millivolts, xrt_core::query::v3v3_pex_milliamps>(device, "3v3_pex", "3.3 Volts PCI Express")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v3v3_aux_millivolts, xrt_core::query::v3v3_aux_milliamps>(device, "3v3_aux", "3.3 Volts Auxillary")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::int_vcc_millivolts, xrt_core::query::int_vcc_milliamps>(device, "vccint", "Internal FPGA Vcc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::int_vcc_io_millivolts, xrt_core::query::int_vcc_io_milliamps>(device, "vccint_io", "Internal FPGA Vcc IO")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::ddr_vpp_bottom_millivolts, xrt_core::query::noop>(device, "ddr_vpp_btm", "DDR Vpp Bottom")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::ddr_vpp_top_millivolts, xrt_core::query::noop>(device, "ddr_vpp_top", "DDR Vpp Top")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v5v5_system_millivolts, xrt_core::query::noop>(device, "5v5_system", "5.5 Volts System")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v1v2_vcc_top_millivolts, xrt_core::query::noop>(device, "1v2_top", "Vcc 1.2 Volts Top")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v1v2_vcc_bottom_millivolts, xrt_core::query::noop>(device, "vcc_1v2_btm", "Vcc 1.2 Volts Bottom")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v1v8_millivolts, xrt_core::query::noop>(device, "1v8_top", "1.8 Volts Top")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v0v9_vcc_millivolts, xrt_core::query::noop>(device, "0v9_vcc", "0.9 Volts Vcc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v12v_sw_millivolts, xrt_core::query::noop>(device, "12v_sw", "12 Volts SW")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::mgt_vtt_millivolts, xrt_core::query::noop>(device, "mgt_vtt", "Mgt Vtt")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v3v3_vcc_millivolts, xrt_core::query::noop>(device, "3v3_vcc", "3.3 Volts Vcc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::hbm_1v2_millivolts, xrt_core::query::noop>(device, "hbm_1v2", "1.2 Volts HBM")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v2v5_vpp_millivolts, xrt_core::query::noop>(device, "vpp2v5", "Vpp 2.5 Volts")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v12_aux1_millivolts, xrt_core::query::noop>(device, "12v_aux1", "12 Volts Aux1")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::noop, xrt_core::query::vcc1v2_i_milliamps>(device, "vcc1v2_i", "Vcc 1.2 Volts i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::noop, xrt_core::query::v12_in_i_milliamps>(device, "v12_in_i", "V12 in i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::noop, xrt_core::query::v12_in_aux0_i_milliamps>(device, "v12_in_aux0_i", "V12 in Aux0 i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::noop, xrt_core::query::v12_in_aux1_i_milliamps>(device, "v12_in_aux1_i", "V12 in Aux1 i")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::vcc_aux_millivolts, xrt_core::query::noop>(device, "vcc_aux", "Vcc Auxillary")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::vcc_aux_pmc_millivolts, xrt_core::query::noop>(device, "vcc_aux_pmc", "Vcc Auxillary Pmc")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::vcc_ram_millivolts, xrt_core::query::noop>(device, "vcc_ram", "Vcc Ram")));
  sensor_array.push_back(std::make_pair("", 
    populate_sensor<xrt_core::query::v0v9_int_vcc_vcu_millivolts, xrt_core::query::noop>(device, "0v9_vccint_vcu", "0.9 Volts Vcc Vcu")));
  root.add_child("power_rails", sensor_array);
  return root;
}


boost::property_tree::ptree
read_thermals(const xrt_core::device * device)
{
  boost::property_tree::ptree root;
  boost::property_tree::ptree thermal_array;
  //--- pcb ----------
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::temp_card_top_front>(device, "pcb_top_front", "PCB Top Front")));
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::temp_card_top_rear>(device, "pcb_top_rear", "PCB Top Rear")));
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::temp_card_bottom_front>(device, "pcb_bottom_front", "PCB Bottom Front")));

  //--- cage ----------
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::cage_temp_0>(device, "cage_temp_0", "Cage0")));
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::cage_temp_1>(device, "cage_temp_1", "Cage1")));
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::cage_temp_2>(device, "cage_temp_2", "Cage2")));
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::cage_temp_3>(device, "cage_temp_3", "Cage3")));

  // --- fpga, vccint, hbm -------------
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::temp_fpga>(device, "fpga0", "FPGA")));
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::int_vcc_temp>(device, "int_vcc", "Int Vcc")));
  thermal_array.push_back(std::make_pair("", populate_temp<xrt_core::query::hbm_temp>(device, "fpga_hbm", "FPGA HBM")));

  root.add_child("thermals", thermal_array);
  return root;
}

boost::property_tree::ptree
read_power_consumption(const xrt_core::device * device)
{
  boost::property_tree::ptree root;
  boost::property_tree::ptree power_array;
  std::string power_watts;
  try {
    auto power_level = xrt_core::device_query<xrt_core::query::max_power_level>(device);
    power_watts = lvl_to_power_watts(power_level);
  }
  catch (...) {
    power_watts = "N/A";
  }
  power_array.put("power_consumption_max_watts", power_watts);
  power_array.put("power_consumption_watts", xrt_core::utils::format_base10_shiftdown6(xrt_core::device_query<xrt_core::query::power_microwatts>(device)));
  power_array.put("power_consumption_warning", xrt_core::query::power_warning::to_string(xrt_core::device_query<xrt_core::query::power_warning>(device)));
  
  root.add_child("power_consumption", power_array);
  return root;
}

boost::property_tree::ptree
read_fans(const xrt_core::device * device)
{
  boost::property_tree::ptree root;
  boost::property_tree::ptree fan_array;
  fan_array.push_back(std::make_pair("", populate_fan(device, "fpga_fan_1", "FPGA Fan 1")));
  
  root.add_child("fans", fan_array);
  return root;
}

}} // sensor,xrt


// ------ L E G A C Y   X B U T I L   F U N C T I O N S  -----------------
// The following namespace is only used by legacy xbutil dump

namespace sensor_tree {

// Singleton
boost::property_tree::ptree&
instance()
{
  static boost::property_tree::ptree s_ptree;
  return s_ptree;
}

void
json_dump(std::ostream &ostr)
{
  boost::property_tree::json_parser::write_json( ostr, instance() );
}

} // namespace sensor_tree
