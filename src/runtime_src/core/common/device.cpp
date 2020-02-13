/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include "device.h"
#include "error.h"
#include "utils.h"
#include "query_requests.h"
#include "core/include/xrt.h"
#include <boost/format.hpp>
#include <string>
#include <iostream>
#include <fstream>

namespace xrt_core {

#if 0  
static std::map<device::QueryRequest, device::QueryRequestEntry> sQueryTable =
{
  { device::QR_XMC_VERSION,               { "QR_XMC_VERSION",               "xmc_version",      &typeid(std::string),  device::format_primative }},
  { device::QR_XMC_SERIAL_NUM,            { "QR_XMC_SERIAL_NUM",            "serial_number",    &typeid(std::string),  device::format_primative }},
  { device::QR_XMC_MAX_POWER,             { "QR_XMC_MAX_POWER",             "max_power",        &typeid(std::string),  device::format_primative }},
  { device::QR_XMC_BMC_VERSION,           { "QR_XMC_BMC_VERSION",           "satellite_controller_version", &typeid(std::string),  device::format_primative }},

  { device::QR_DNA_SERIAL_NUM,            { "QR_DNA_SERIAL_NUM",            "dna",              &typeid(std::string),  device::format_primative }},

  { device::QR_STATUS_P2P_ENABLED,        { "QR_STATUS_P2P_ENABLED",        "p2p_enabled",      &typeid(bool),  device::format_primative }},

  { device::QR_TEMP_CARD_TOP_FRONT,       { "QR_TEMP_CARD_TOP_FRONT",       "temp_top_front_C",    &typeid(uint64_t),  device::format_primative }},
  { device::QR_TEMP_CARD_TOP_REAR,        { "QR_TEMP_CARD_TOP_REAR",        "temp_top_rear_C",     &typeid(uint64_t),  device::format_primative }},
  { device::QR_TEMP_CARD_BOTTOM_FRONT,    { "QR_TEMP_CARD_BOTTOM_FRONT",    "temp_bottom_front_C", &typeid(uint64_t),  device::format_primative }},

  { device::QR_TEMP_FPGA,                 { "QR_TEMP_FPGA",                 "temp_C",           &typeid(uint64_t),  device::format_primative }},

  { device::QR_FAN_TRIGGER_CRITICAL_TEMP, { "QR_FAN_TRIGGER_CRITICAL_TEMP", "temp_trigger_critical_C",  &typeid(uint64_t),  device::format_primative }},
  { device::QR_FAN_FAN_PRESENCE,          { "QR_FAN_FAN_PRESENCE",          "fan_presence",             &typeid(std::string),  device::format_primative }},
  { device::QR_FAN_SPEED_RPM,             { "QR_FAN_SPEED_RPM",             "fan_speed_rpm",            &typeid(uint64_t),  device::format_primative }},

  { device::QR_CAGE_TEMP_0,               { "QR_CAGE_TEMP_0",               "temp0_C",          &typeid(uint64_t),  device::format_primative }},
  { device::QR_CAGE_TEMP_1,               { "QR_CAGE_TEMP_1",               "temp1_C",          &typeid(uint64_t),  device::format_primative }},
  { device::QR_CAGE_TEMP_2,               { "QR_CAGE_TEMP_2",               "temp2_C",          &typeid(uint64_t),  device::format_primative }},
  { device::QR_CAGE_TEMP_3,               { "QR_CAGE_TEMP_3",               "temp3_C",          &typeid(uint64_t),  device::format_primative }},

  { device::QR_12V_PEX_MILLIVOLTS,        { "QR_12V_PEX_MILLIVOLTS",        "12v_pex.voltage",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_12V_PEX_MILLIAMPS,         { "QR_12V_PEX_MILLIAMPS",         "12v_pex.current",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_12V_AUX_MILLIVOLTS,        { "QR_12V_AUX_MILLIVOLTS",        "12v_aux.voltage",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_12V_AUX_MILLIAMPS,         { "QR_12V_AUX_MILLIAMPS",         "12v_aux.current",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},

  { device::QR_3V3_PEX_MILLIVOLTS,        { "QR_3V3_PEX_MILLIVOLTS",        "3v3_pex.voltaget", &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_3V3_AUX_MILLIVOLTS,        { "QR_3V3_AUX_MILLIVOLTS",        "3v3_aux.voltage",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},

  { device::QR_DDR_VPP_BOTTOM_MILLIVOLTS, { "QR_DDR_VPP_BOTTOM_MILLIVOLTS", "ddr_vpp_bottom.voltage", &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_DDR_VPP_TOP_MILLIVOLTS,    { "QR_DDR_VPP_TOP_MILLIVOLTS",    "ddr_vpp_top.voltage",    &typeid(uint64_t),  device::format_base10_shiftdown3 }},

  { device::QR_5V5_SYSTEM_MILLIVOLTS,     { "QR_5V5_SYSTEM_MILLIVOLTS",    "sys_5v5.voltage",   &typeid(uint64_t),  device::format_base10_shiftdown3 }},

  { device::QR_1V2_VCC_TOP_MILLIVOLTS,    { "QR_1V2_VCC_TOP_MILLIVOLTS",    "1v2_top.voltage",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_1V2_VCC_BOTTOM_MILLIVOLTS, { "QR_1V2_VCC_BOTTOM_MILLIVOLTS", "1v2_btm.voltage",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},

  { device::QR_0V85_MILLIVOLTS,           { "QR_0V85_MILLIVOLTS",           "0v85.voltage",     &typeid(uint64_t),  device::format_base10_shiftdown3 }},

  { device::QR_1V8_MILLIVOLTS,            { "QR_1V8_MILLIVOLTS",            "1v8.voltage",      &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_0V9_VCC_MILLIVOLTS,        { "QR_0V9_VCC_MILLIVOLTS",        "mgt_0v9.voltage",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_12V_SW_MILLIVOLTS,         { "QR_12V_SW_MILLIVOLTS",         "12v_sw.voltage",   &typeid(uint64_t),  device::format_base10_shiftdown3 }},

  { device::QR_MGT_VTT_MILLIVOLTS,        { "QR_MGT_VTT_MILLIVOLTS",        "mgt_vtt.voltage",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_INT_VCC_MILLIVOLTS,        { "QR_INT_VCC_MILLIVOLTS",        "vccint.voltage",   &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_INT_VCC_MILLIAMPS,         { "QR_INT_VCC_MILLIAMPS",         "vccint.current",   &typeid(uint64_t),  device::format_base10_shiftdown3 }},

  { device::QR_3V3_PEX_MILLIAMPS,         { "QR_3V3_PEX_MILLIAMPS",         "3v3_pex.current",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_0V85_MILLIAMPS,            { "QR_0V85_MILLIAMPS",            "0v85.current",     &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_3V3_VCC_MILLIVOLTS,        { "QR_3V3_VCC_MILLIVOLTS",        "vcc3v3.voltage",   &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_HBM_1V2_MILLIVOLTS,        { "QR_HBM_1V2_MILLIVOLTS",        "hbm_1v2.voltage",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_2V5_VPP_MILLIVOLTS,        { "QR_2V5_VPP_MILLIVOLTS",        "vpp2v5.voltage",   &typeid(uint64_t),  device::format_base10_shiftdown3 }},
  { device::QR_INT_BRAM_VCC_MILLIVOLTS,   { "QR_INT_BRAM_VCC_MILLIVOLTS",   "vccint_bram.voltage",  &typeid(uint64_t),  device::format_base10_shiftdown3 }},

  { device::QR_FIREWALL_DETECT_LEVEL,     { "QR_FIREWALL_DETECT_LEVEL",     "level",            &typeid(uint64_t),  device::format_primative }},
  { device::QR_FIREWALL_STATUS,           { "QR_FIREWALL_STATUS",           "status",           &typeid(uint64_t),  device::format_hex }},
  { device::QR_FIREWALL_TIME_SEC,         { "QR_FIREWALL_TIME_SEC",         "time_sec",         &typeid(uint64_t),  device::format_primative }},

  { device::QR_POWER_MICROWATTS,          { "QR_POWER_MICROWATTS",          "power_watts",      &typeid(uint64_t),  device::format_base10_shiftdown6 }}
};
#endif

device::device(id_type device_id)
  : m_device_id(device_id)
{
}

device::~device()
{
  // virtual must be declared and defined
}

std::string
device::format_primative(const boost::any &_data)
{
  std::string sPropertyValue;

  if (_data.type() == typeid(std::string)) {
    sPropertyValue = boost::any_cast<std::string>(_data);
  }
  else if (_data.type() == typeid(uint64_t)) {
	  sPropertyValue = std::to_string(boost::any_cast<uint64_t>(_data));
  } else if (_data.type() == typeid(uint16_t)) {
	  sPropertyValue = std::to_string(boost::any_cast<uint16_t>(_data));
  } else if (_data.type() == typeid(bool)) {
    sPropertyValue = boost::any_cast<bool>(_data) ? "true" : "false";
  }
  else {
    std::string errMsg = boost::str( boost::format("Unsupported 'any' typeid: '%s'") % _data.type().name());
    throw error(errMsg);
  }

  return sPropertyValue;
}

std::string
device::format_hex(const boost::any & _data)
{
  // Can we work with this data type?
  if (_data.type() == typeid(uint64_t))
    return boost::str(boost::format("0x%x") % boost::any_cast<uint64_t>(_data));
  if (_data.type() == typeid(uint16_t))
    return boost::str(boost::format("0x%x") % boost::any_cast<uint16_t>(_data));
  if (_data.type() == typeid(uint8_t))
    return boost::str(boost::format("0x%x") % boost::any_cast<uint8_t>(_data));
  return format_primative(_data);
}

template <typename T>
std::string
static to_string(const T _value, const int _precision = 6)
{
  std::ostringstream sBuffer;
  sBuffer.precision(_precision);
  sBuffer << std::fixed << _value;
  return sBuffer.str();
}

std::string
device::format_base10_shiftdown3(const boost::any &_data)
{
  if (_data.type() != typeid(uint64_t)) {
    return format_primative(_data);
  }

  double value = (double) boost::any_cast<uint64_t>(_data);
  value /= (double) 1000.0;    // Shift down 3
  return to_string(value, 3 /*precision*/ );
}

std::string
device::format_base10_shiftdown6(const boost::any &_data)
{
  if (_data.type() != typeid(uint64_t)) {
    return format_primative(_data);
  }

  double value = (double) boost::any_cast<uint64_t>(_data);
  value /= (double) 1000000.0;    // Shift down 3
  return to_string(value, 6 /*precision*/ );
}

std::string
device::format_hex_base2_shiftup30(const boost::any &_data)
{
  if (_data.type() == typeid(uint64_t)) {
    boost::any modifiedValue = boost::any_cast<uint64_t>(_data) << 30;
    return format_hex(modifiedValue);
  }
  if (_data.type() == typeid(uint16_t)) {
    boost::any modifiedValue = boost::any_cast<uint16_t>(_data) << 30;
    return format_hex(modifiedValue);
  }
  if (_data.type() == typeid(uint8_t)) {
    boost::any modifiedValue = boost::any_cast<uint8_t>(_data) << 30;
    return format_hex(modifiedValue);
  }
  return format_primative(_data);
}

void
device::
get_rom_info(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::rom_vbnv>::query_and_put(this, pt);
  ptree_updater<query::rom_ddr_bank_size>::query_and_put(this, pt);
  ptree_updater<query::rom_ddr_bank_count_max>::query_and_put(this, pt);
  ptree_updater<query::rom_fpga_name>::query_and_put(this, pt);
}


void
device::
get_xmc_info(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::xmc_version>::query_and_put(this, pt);
  ptree_updater<query::xmc_serial_num>::query_and_put(this, pt);
  ptree_updater<query::xmc_max_power>::query_and_put(this, pt);
  ptree_updater<query::xmc_bmc_version>::query_and_put(this, pt);
}

void
device::
get_platform_info(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::dna_serial_num>::query_and_put(this, pt);
  ptree_updater<query::clock_freqs>::query_and_put(this, pt);
  ptree_updater<query::idcode>::query_and_put(this, pt);
  ptree_updater<query::status_mig_calibrated>::query_and_put(this, pt);
  ptree_updater<query::status_p2p_enabled>::query_and_put(this, pt);
}

void
device::
read_thermal_pcb(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::temp_card_top_front>::query_and_put(this, pt);
  ptree_updater<query::temp_card_top_rear>::query_and_put(this, pt);
  ptree_updater<query::temp_card_bottom_front>::query_and_put(this, pt);
}

void
device::
read_thermal_fpga(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::temp_fpga>::query_and_put(this, pt);
}

void
device::
read_fan_info(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::fan_trigger_critical_temp>::query_and_put(this, pt);
  ptree_updater<query::fan_fan_presence>::query_and_put(this, pt);
  ptree_updater<query::fan_speed_rpm>::query_and_put(this, pt);
}

void
device::
read_thermal_cage(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::cage_temp_0>::query_and_put(this, pt);
  ptree_updater<query::cage_temp_1>::query_and_put(this, pt);
  ptree_updater<query::cage_temp_2>::query_and_put(this, pt);
  ptree_updater<query::cage_temp_3>::query_and_put(this, pt);
}

void
device::
read_electrical(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::v12v_pex_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v12v_pex_milliamps>::query_and_put(this,  pt);
  ptree_updater<query::v12v_aux_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v12v_aux_milliamps>::query_and_put(this,  pt);

  ptree_updater<query::v3v3_pex_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v3v3_aux_millivolts>::query_and_put(this, pt);
  ptree_updater<query::ddr_vpp_bottom_millivolts>::query_and_put(this, pt);
  ptree_updater<query::ddr_vpp_top_millivolts>::query_and_put(this, pt);


  ptree_updater<query::v5v5_system_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v1v2_vcc_top_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v1v2_vcc_bottom_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v1v8_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v0v85_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v0v9_vcc_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v12v_sw_millivolts>::query_and_put(this, pt);
  ptree_updater<query::mgt_vtt_millivolts>::query_and_put(this, pt);
  ptree_updater<query::int_vcc_millivolts>::query_and_put(this, pt);
  ptree_updater<query::int_vcc_milliamps>::query_and_put(this, pt);

  ptree_updater<query::v3v3_pex_milliamps>::query_and_put(this, pt);
  ptree_updater<query::v0v85_milliamps>::query_and_put(this, pt);
  ptree_updater<query::v3v3_vcc_millivolts>::query_and_put(this, pt);
  ptree_updater<query::hbm_1v2_millivolts>::query_and_put(this, pt);
  ptree_updater<query::v2v5_vpp_millivolts>::query_and_put(this, pt);
  ptree_updater<query::int_bram_vcc_millivolts>::query_and_put(this, pt);
}

void
device::
read_power(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::power_microwatts>::query_and_put(this, pt);
}


void
device::
read_firewall(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::firewall_detect_level>::query_and_put(this, pt);
  ptree_updater<query::firewall_status>::query_and_put(this, pt);
  ptree_updater<query::firewall_time_sec>::query_and_put(this, pt);
}

} // xrt_core
