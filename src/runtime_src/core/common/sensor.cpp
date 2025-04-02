/**
 * Copyright (C) 2018-2022 Xilinx, Inc
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
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
#define XRT_CORE_COMMON_SOURCE
#include "sensor.h"
#include "query_requests.h"
#include "utils.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>

#include <sstream>
#include <iomanip>

// Too much typing
using ptree_type = boost::property_tree::ptree;
namespace xq = xrt_core::query;


namespace {
// Saves voltage-current pair of a sensor into a boost::property_tree
// Converts mV and mA into V and A before adding to the tree
//
// @tparam QRVoltage voltage query for a sensor; query::noop if query DNE
// @tparam QRCurrent current query for the same sensor; query::noop if query DNE
// @param loc_id human readable sensor identifier
// @desc description about the sensor
template <typename QRVoltage, typename QRCurrent>
static ptree_type
populate_sensor(const xrt_core::device * device,
                const std::string& loc_id,
                const std::string& desc)
{
  ptree_type pt;
  pt.put("id", loc_id);
  pt.put("description", desc);

  uint64_t voltage = 0;
  uint64_t current = 0;
  try {
    if (!std::is_same<QRVoltage, xq::noop>::value)
      voltage = xrt_core::device_query<QRVoltage>(device);
  }
  catch (const std::exception& ex) {
    pt.put("voltage.error_msg", ex.what());
  }
  pt.put("voltage.volts", xrt_core::utils::format_base10_shiftdown3(voltage));
  pt.put("voltage.is_present", voltage != 0 ? "true" : "false");

  try {
    if (!std::is_same<QRCurrent, xq::noop>::value)
      current = xrt_core::device_query<QRCurrent>(device);
  }
  catch (const std::exception& ex) {
    pt.put("current.error_msg", ex.what());
  }
  pt.put("current.amps", xrt_core::utils::format_base10_shiftdown3(current));
  pt.put("current.is_present", current != 0 ? "true" : "false");

  return pt;
}

template <typename QueryRequestType>
static ptree_type
populate_temp(const xrt_core::device * device,
              const std::string& loc_id,
              const std::string& desc)
{
  ptree_type pt;
  uint64_t temp_C = 0;
  try {
    temp_C = xrt_core::device_query<QueryRequestType>(device);
  }
  catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
  }

  pt.put("location_id", loc_id);
  pt.put("description", desc);
  pt.put("temp_C", temp_C);
  pt.put("is_present", temp_C != 0 ? "true" : "false");

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

static ptree_type
populate_fan(const xrt_core::device * device,
             const std::string& loc_id,
             const std::string& desc)
{
  ptree_type pt;
  uint64_t temp_C = 0;
  uint64_t rpm = 0;
  std::string is_present;
  try {
    temp_C = xrt_core::device_query<xq::fan_trigger_critical_temp>(device);
    rpm = xrt_core::device_query<xq::fan_speed_rpm>(device);
    is_present = xrt_core::device_query<xq::fan_fan_presence>(device);
  }
  catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
  }

  pt.put("location_id", loc_id);
  pt.put("description", desc);
  pt.put("critical_trigger_temp_C", temp_C);
  pt.put("speed_rpm", rpm);
  pt.put("is_present", xq::fan_fan_presence::to_string(is_present));

  return pt;
}

/*
 * _data_driven_*(): Sensor data driven model APIs
 *   without it's name and it is not static.
 *   The data is being accessed from hwmon driver's sysfs nodes, which
 *   are registered by xrt client device driver.
 * Input:
 *   Input to these functions are vector of "structure sensor_data" type.
 * Output:
 *   This function iterates over the input vector and prepares property tree.
 *   Converts mV, mA & mW into V, A & W before adding to the tree
 */
static ptree_type
read_data_driven_electrical(const std::vector<xq::sdm_sensor_info::data_type>& current,
                            const std::vector<xq::sdm_sensor_info::data_type>& voltage,
                            const std::vector<xq::sdm_sensor_info::data_type>& power)
{
  ptree_type sensor_array;
  ptree_type pt;

  // iterate over current data, store to ptree by converting to Amps from milli Amps
  for (const auto& tmp : voltage) {
    pt.put("id", tmp.label);
    pt.put("description", tmp.label);
    /*
     * Use below calculation for sensor values:
     * actual sensor value = sensor_value * (10 ^ (unit_modifier))
     * Example: Sensor Value 12000, Units “Volts” & Unit Modifier -3 received.
     * So, actual sensor value => 12000 * 10 ^ (-3) = 12 Volts.
     */
    pt.put("voltage.volts", xrt_core::utils::format_base10_shiftdown(tmp.input, tmp.unitm, 3));
    pt.put("voltage.max", xrt_core::utils::format_base10_shiftdown(tmp.max, tmp.unitm, 3));
    pt.put("voltage.average", xrt_core::utils::format_base10_shiftdown(tmp.average, tmp.unitm, 3));
    // these fields are also needed to differentiate between sensor types
    pt.put("voltage.is_present", "true");
    pt.put("current.is_present", "false");
    sensor_array.push_back({"", pt});
  }

  // iterate over voltage data, store to ptree by converting to Volts from milli Volts
  for (const auto& tmp : current) {
    bool found =false;
    auto desc = tmp.label;
    auto amps = xrt_core::utils::format_base10_shiftdown(tmp.input, tmp.unitm, 3);
    auto max = xrt_core::utils::format_base10_shiftdown(tmp.max, tmp.unitm, 3);
    auto avg = xrt_core::utils::format_base10_shiftdown(tmp.average, tmp.unitm, 3);

    for (auto& kv : sensor_array) {
      auto id = kv.second.get<std::string>("id");
      if (desc.find(id) != std::string::npos)
      {
        kv.second.put("current.amps", amps);
        kv.second.put("current.max", max);
        kv.second.put("current.average", avg);
        kv.second.put("current.is_present", "true");
        found = true;
        break;
      }
    }

    if (found)
      continue;

    pt.put("id", tmp.label);
    pt.put("description", tmp.label);
    pt.put("current.amps", amps);
    pt.put("current.max", max);
    pt.put("current.average", avg);
    // these fields are also needed to differentiate between sensor types
    pt.put("current.is_present", "true");
    pt.put("voltage.is_present", "false");
    sensor_array.push_back({"", pt});
  }

  std::string bd_power = "N/A";
  std::string bd_max_power = "N/A";
  // iterate over power data, store to ptree by converting to watts.
  for (const auto& tmp : power) {
    if (boost::iequals(tmp.label, "Total Power")) {
      if ((tmp.input != std::numeric_limits<decltype(xq::sdm_sensor_info::data_type::input)>::max()) && (tmp.input != 0))
        bd_power = xrt_core::utils::format_base10_shiftdown(tmp.input, tmp.unitm, 3);
      bd_max_power = xrt_core::utils::format_base10_shiftdown(tmp.max, tmp.unitm, 3);
    }
  }
  ptree_type root;

  root.add_child("power_rails", sensor_array);

  root.put("power_consumption_watts", bd_power);
  root.put("power_consumption_max_watts", bd_max_power);
  root.put("power_consumption_warning", "N/A");

  return root;
}

static ptree_type
read_data_driven_thermals(const std::vector<xq::sdm_sensor_info::data_type>& output)
{
  ptree_type thermal_array;
  ptree_type root;

  // iterate over temperature data, store to ptree by converting to Celcius
  for (const auto& tmp : output) {
    ptree_type pt;
    pt.put("location_id", tmp.label);
    pt.put("description", tmp.label);
    pt.put("temp_C", tmp.input);
    pt.put("is_present", "true");
    thermal_array.push_back({"", pt});
  }

  root.add_child("thermals", thermal_array);
  return root;
}

static ptree_type
read_data_driven_mechanical(std::vector<xq::sdm_sensor_info::data_type>& output)
{
  ptree_type root;
  ptree_type pt;
  ptree_type fan_array;

  // iterate over output data, store it into property_tree
  for (const auto& tmp : output) {
    pt.put("location_id", tmp.label);
    pt.put("description", tmp.label);
    pt.put("critical_trigger_temp_C", "N/A");
    pt.put("speed_rpm", tmp.input);
    pt.put("is_present", "true");
    fan_array.push_back({"", pt});
  }

  root.add_child("fans", fan_array);
  return root;
}

/*
 * *_legacy_*() functions:
 *
 * Use these functions to get sensor data using their name.
 * Input:
 *   device handle
 * Output:
 *   Access sensor data using it's name & proper device handle.
 *   Store the retrieved sensor data into boost::property_tree
 *   and return the same.
 */

static ptree_type
read_legacy_mechanical(const xrt_core::device * device)
{
  ptree_type root;
  ptree_type fan_array;

  fan_array.push_back({"", populate_fan(device, "fpga_fan_1", "FPGA Fan 1")});

  root.add_child("fans", fan_array);
  return root;
}

static ptree_type
read_legacy_thermals(const xrt_core::device * device)
{
  ptree_type thermal_array;
  ptree_type root;

  //--- pcb ----------
  thermal_array.push_back({"",
	populate_temp<xq::temp_card_top_front>(device, "pcb_top_front", "PCB Top Front")});
  thermal_array.push_back({"",
	populate_temp<xq::temp_card_top_rear>(device, "pcb_top_rear", "PCB Top Rear")});
  thermal_array.push_back({"",
	populate_temp<xq::temp_card_bottom_front>(device, "pcb_bottom_front", "PCB Bottom Front")});

  //--- cage ----------
  thermal_array.push_back({"",
	populate_temp<xq::cage_temp_0>(device, "cage_temp_0", "Cage0")});
  thermal_array.push_back({"",
	populate_temp<xq::cage_temp_1>(device, "cage_temp_1", "Cage1")});
  thermal_array.push_back({"",
	populate_temp<xq::cage_temp_2>(device, "cage_temp_2", "Cage2")});
  thermal_array.push_back({"",
	populate_temp<xq::cage_temp_3>(device, "cage_temp_3", "Cage3")});

  // --- fpga, vccint, hbm -------------
  thermal_array.push_back({"",
	populate_temp<xq::temp_fpga>(device, "fpga0", "FPGA")});
  thermal_array.push_back({"",
	populate_temp<xq::int_vcc_temp>(device, "int_vcc", "Int Vcc")});
  thermal_array.push_back({"",
	populate_temp<xq::hbm_temp>(device, "fpga_hbm", "FPGA HBM")});

  root.add_child("thermals", thermal_array);
  return root;
}

static ptree_type
read_legacy_electrical(const xrt_core::device * device)
{
  ptree_type sensor_array;
  ptree_type pt;

  sensor_array.push_back({"",
    populate_sensor<xq::v12v_aux_millivolts, xq::v12v_aux_milliamps>(device, "12v_aux", "12 Volts Auxillary")});
  sensor_array.push_back({"",
    populate_sensor<xq::v12v_pex_millivolts, xq::v12v_pex_milliamps>(device, "12v_pex", "12 Volts PCI Express")});
  sensor_array.push_back({"",
    populate_sensor<xq::v3v3_pex_millivolts, xq::v3v3_pex_milliamps>(device, "3v3_pex", "3.3 Volts PCI Express")});
  sensor_array.push_back({"",
    populate_sensor<xq::v3v3_aux_millivolts, xq::v3v3_aux_milliamps>(device, "3v3_aux", "3.3 Volts Auxillary")});

  /* Board power measurement uses cached values of above sensors.*/
  std::string power_watts;
  std::string power_warn;
  std::string max_power_watts;
  try {
    power_watts = xrt_core::utils::format_base10_shiftdown6(xrt_core::device_query<xq::power_microwatts>(device));
    power_warn = xq::power_warning::to_string(xrt_core::device_query<xq::power_warning>(device));
    auto power_level = xrt_core::device_query<xq::max_power_level>(device);
    max_power_watts = lvl_to_power_watts(power_level);
  }
  catch (const xq::exception&) {
    power_watts = "N/A";
    power_warn = "N/A";
    max_power_watts = "N/A";
  }

  sensor_array.push_back({"",
    populate_sensor<xq::int_vcc_millivolts, xq::int_vcc_milliamps>(device, "vccint", "Internal FPGA Vcc")});
  sensor_array.push_back({"",
    populate_sensor<xq::int_vcc_io_millivolts, xq::int_vcc_io_milliamps>(device, "vccint_io", "Internal FPGA Vcc IO")});
  sensor_array.push_back({"",
    populate_sensor<xq::ddr_vpp_bottom_millivolts, xq::noop>(device, "ddr_vpp_btm", "DDR Vpp Bottom")});
  sensor_array.push_back({"",
    populate_sensor<xq::ddr_vpp_top_millivolts, xq::noop>(device, "ddr_vpp_top", "DDR Vpp Top")});
  sensor_array.push_back({"",
    populate_sensor<xq::v5v5_system_millivolts, xq::noop>(device, "5v5_system", "5.5 Volts System")});
  sensor_array.push_back({"",
    populate_sensor<xq::v1v2_vcc_top_millivolts, xq::noop>(device, "1v2_top", "Vcc 1.2 Volts Top")});
  sensor_array.push_back({"",
    populate_sensor<xq::v1v2_vcc_bottom_millivolts, xq::noop>(device, "vcc_1v2_btm", "Vcc 1.2 Volts Bottom")});
  sensor_array.push_back({"",
    populate_sensor<xq::v1v8_millivolts, xq::noop>(device, "1v8_top", "1.8 Volts Top")});
  sensor_array.push_back({"",
    populate_sensor<xq::v0v9_vcc_millivolts, xq::noop>(device, "0v9_vcc", "0.9 Volts Vcc")});
  sensor_array.push_back({"",
    populate_sensor<xq::v12v_sw_millivolts, xq::noop>(device, "12v_sw", "12 Volts SW")});
  sensor_array.push_back({"",
    populate_sensor<xq::mgt_vtt_millivolts, xq::noop>(device, "mgt_vtt", "Mgt Vtt")});
  sensor_array.push_back({"",
    populate_sensor<xq::v3v3_vcc_millivolts, xq::noop>(device, "3v3_vcc", "3.3 Volts Vcc")});
  sensor_array.push_back({"",
    populate_sensor<xq::hbm_1v2_millivolts, xq::noop>(device, "hbm_1v2", "1.2 Volts HBM")});
  sensor_array.push_back({"",
    populate_sensor<xq::v2v5_vpp_millivolts, xq::noop>(device, "vpp2v5", "Vpp 2.5 Volts")});
  sensor_array.push_back({"",
    populate_sensor<xq::v12_aux1_millivolts, xq::noop>(device, "12v_aux1", "12 Volts Aux1")});
  sensor_array.push_back({"",
    populate_sensor<xq::noop, xq::vcc1v2_i_milliamps>(device, "vcc1v2_i", "Vcc 1.2 Volts i")});
  sensor_array.push_back({"",
    populate_sensor<xq::noop, xq::v12_in_i_milliamps>(device, "v12_in_i", "V12 in i")});
  sensor_array.push_back({"",
    populate_sensor<xq::noop, xq::v12_in_aux0_i_milliamps>(device, "v12_in_aux0_i", "V12 in Aux0 i")});
  sensor_array.push_back({"",
    populate_sensor<xq::noop, xq::v12_in_aux1_i_milliamps>(device, "v12_in_aux1_i", "V12 in Aux1 i")});
  sensor_array.push_back({"",
    populate_sensor<xq::vcc_aux_millivolts, xq::noop>(device, "vcc_aux", "Vcc Auxillary")});
  sensor_array.push_back({"",
    populate_sensor<xq::vcc_aux_pmc_millivolts, xq::noop>(device, "vcc_aux_pmc", "Vcc Auxillary Pmc")});
  sensor_array.push_back({"",
    populate_sensor<xq::vcc_ram_millivolts, xq::noop>(device, "vcc_ram", "Vcc Ram")});
  sensor_array.push_back({"",
    populate_sensor<xq::v0v9_int_vcc_vcu_millivolts, xq::noop>(device, "0v9_vccint_vcu", "0.9 Volts Vcc Vcu")});

  ptree_type root;
  root.add_child("power_rails", sensor_array);
  root.put("power_consumption_max_watts", max_power_watts);
  root.put("power_consumption_watts", power_watts);
  root.put("power_consumption_warning", power_warn);

  return root;
}

} //unnamed namespace

namespace xrt_core { namespace sensor {

/*
 * read_<>() functions are top level functions are being called from tools/common driver.
 * Job is to get all the requested sensor information stored into boost::property_tree.
 * All these functions follow below steps:
 *   1) It first check if the sensor data can be accessed in data driven model.
 *      Uses xrt query "sdm_sensor_info" to get the sensors of data driven mode.
 *   2) If the received sensor list is not empty then it calls _data_driven_<>() functions
 *      and stores the data into property_tree.
 *      If received list is empty goto step (3)
 *   3) If the received sensors list is empty then it calls _legacy_<>() functions
 *      to access the sensors using their property like name.
 */
ptree_type
read_electrical(const xrt_core::device * device)
{
  ptree_type sensor_array;
  bool is_data_driven = true;

  //Check if requested sensor data can be retrieved in data driven model.
  try {
    auto current  = xrt_core::device_query<xq::sdm_sensor_info>(device, xq::sdm_sensor_info::sdr_req_type::current);
    auto voltage  = xrt_core::device_query<xq::sdm_sensor_info>(device, xq::sdm_sensor_info::sdr_req_type::voltage);
    auto power  = xrt_core::device_query<xq::sdm_sensor_info>(device, xq::sdm_sensor_info::sdr_req_type::power);
    //Check for any of these data is available
    if (!current.empty() || !voltage.empty() || !power.empty())
      return read_data_driven_electrical(current, voltage, power);
    else {
      sensor_array.put("msg", "No sensors present");
      return sensor_array;
    }
  }
  catch(const xrt_core::query::no_such_key&) {
    is_data_driven = false;
  }
  catch(const xrt_core::query::exception&) {
    is_data_driven = false;
  }
  catch (const std::exception& ex) {
    sensor_array.push_back({"", sensor_array.put("error_msg", ex.what())});
  }

  if (!is_data_driven)
    return read_legacy_electrical(device);

  return sensor_array;
}

ptree_type
read_thermals(const xrt_core::device * device)
{
  ptree_type root;
  ptree_type thermal_array;
  ptree_type pt;
  bool is_data_driven = true;
  std::vector <xq::sdm_sensor_info::data_type> output;

  //Check if requested sensor data can be retrieved in data driven model.
  try {
    output = xrt_core::device_query<xq::sdm_sensor_info>(device, xq::sdm_sensor_info::sdr_req_type::thermal);
    if (output.empty()) {
      thermal_array.put("msg", "No sensors present");
      root.add_child("thermals", thermal_array);
      return root;
    }
  }
  catch(const xrt_core::query::no_such_key&) {
    is_data_driven = false;
  }
  catch(const xrt_core::query::exception&) {
    is_data_driven = false;
  }
  catch (const std::exception& ex) {
    thermal_array.push_back({"", thermal_array.put("error_msg", ex.what())});
    root.add_child("thermals", thermal_array);
    return root;
  }

  if (is_data_driven)
    return read_data_driven_thermals(output);
  else
    return read_legacy_thermals(device);
}

ptree_type
read_mechanical(const xrt_core::device * device)
{
  ptree_type root;
  ptree_type fan_array;
  bool is_data_driven = true;
  std::vector <xq::sdm_sensor_info::data_type> output;

  //Check if requested sensor data can be retrieved in data driven model.
  try {
    output = xrt_core::device_query<xq::sdm_sensor_info>(device, xq::sdm_sensor_info::sdr_req_type::mechanical);
    if (output.empty()) {
      fan_array.put("msg", "No sensors present");
      root.add_child("fans", fan_array);
      return root;
    }
  }
  catch(const xrt_core::query::no_such_key&) {
    is_data_driven = false;
  }
  catch(const xrt_core::query::exception&) {
    is_data_driven = false;
  }
  catch (const std::exception& ex) {
    fan_array.push_back({"", fan_array.put("error_msg", ex.what())});
    root.add_child("fans", fan_array);
    return root;
  }

  if (is_data_driven)
    return read_data_driven_mechanical(output);
  else
    return read_legacy_mechanical(device);
}

}} // sensor,xrt


// The following namespace is only used by legacy xbutil dump
namespace sensor_tree {

// Singleton
ptree_type&
instance()
{
  static ptree_type s_ptree;
  return s_ptree;
}

void
json_dump(std::ostream &ostr)
{
  boost::property_tree::json_parser::write_json( ostr, instance() );
}

} // namespace sensor_tree
