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

#include "device.h"
#include "error.h"
#include "utils.h"
#include "include/xrt.h"
#include "boost/format.hpp"
#include <string>
#include <iostream>
#include <fstream>

namespace xrt_core {

static std::map<device::QueryRequest, device::QueryRequestEntry> sQueryTable =
{
  { device::QR_PCIE_VENDOR,               { "QR_PCIE_VENDOR",               "vendor",           &typeid(std::string), device::format_primative }},
  { device::QR_PCIE_DEVICE,               { "QR_PCIE_DEVICE",               "device",           &typeid(std::string), device::format_primative }},
  { device::QR_PCIE_SUBSYSTEM_VENDOR,     { "QR_PCIE_SUBSYSTEM_VENDOR",     "subsystem_vendor", &typeid(std::string), device::format_primative }},
  { device::QR_PCIE_SUBSYSTEM_ID,         { "QR_PCIE_SUBSYSTEM_ID",         "subsystem_id",     &typeid(std::string), device::format_primative }},
  { device::QR_PCIE_LINK_SPEED,           { "QR_PCIE_LINK_SPEED",           "link_speed",       &typeid(uint64_t),    device::format_primative }},
  { device::QR_PCIE_EXPRESS_LANE_WIDTH,   { "QR_PCIE_EXPRESS_LANE_WIDTH",   "width",            &typeid(uint64_t),    device::format_primative }},

  { device::QR_ROM_VBNV,                  { "QR_ROM_VBNV",                  "vbnv",             &typeid(std::string), device::format_primative }},
  { device::QR_ROM_DDR_BANK_SIZE,         { "QR_ROM_DDR_BANK_SIZE",         "ddr_size_bytes",   &typeid(uint64_t),    device::format_hex_base2_shiftup30 }},
  { device::QR_ROM_DDR_BANK_COUNT_MAX,    { "QR_ROM_DDR_BANK_COUNT_MAX",    "widdr_countdth",   &typeid(uint64_t),    device::format_primative }},
  { device::QR_ROM_FPGA_NAME,             { "QR_ROM_FPGA_NAME",             "fpga_name",        &typeid(std::string), device::format_primative }},
  { device::QR_DMA_THREADS_RAW,           { "QR_DMA_THREADS_RAW",           "dma_threads",      &typeid(std::vector<std::string>),  device::format_primative }},

  { device::QR_XMC_VERSION,               { "QR_XMC_VERSION",               "xmc_version",      &typeid(std::string),  device::format_primative }},
  { device::QR_XMC_SERIAL_NUM,            { "QR_XMC_SERIAL_NUM",            "serial_number",    &typeid(std::string),  device::format_primative }},
  { device::QR_XMC_MAX_POWER,             { "QR_XMC_MAX_POWER",             "max_power",        &typeid(std::string),  device::format_primative }},
  { device::QR_XMC_BMC_VERSION,           { "QR_XMC_BMC_VERSION",           "satellite_controller_version", &typeid(std::string),  device::format_primative }},

  { device::QR_DNA_SERIAL_NUM,            { "QR_DNA_SERIAL_NUM",            "dna",              &typeid(std::string),  device::format_primative }},
  { device::QR_CLOCK_FREQS,               { "QR_CLOCK_FREQS",               "clocks",           &typeid(std::vector<std::string>),  device::format_primative }},
  { device::QR_IDCODE,                    { "QR_IDCODE",                    "idcode",           &typeid(std::string),  device::format_primative }},

  { device::QR_STATUS_MIG_CALIBRATED,     { "QR_STATUS_MIG_CALIBRATED",     "mig_calibrated",   &typeid(bool),  device::format_primative }},
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


const device::QueryRequestEntry *
device::
get_query_entry(QueryRequest qr) const
{
  auto it = sQueryTable.find(qr);

  if (it == sQueryTable.end()) {
    std::string errMsg = boost::str( boost::format("The given query request ID (%d) was dont found.") % qr);
    throw no_such_query(qr, errMsg);
  }

  return &((*it).second);
}

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
  } else if (_data.type() == typeid(uint64_t)) {
    sPropertyValue = std::to_string(boost::any_cast<uint64_t>(_data));
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
  if (_data.type() != typeid(uint64_t)) {
    return format_primative(_data);
  }

  return boost::str( boost::format("0x%x") % boost::any_cast<uint64_t>(_data));
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
  if (_data.type() != typeid(uint64_t)) {
    return format_primative(_data);
  }

  boost::any modifiedValue = boost::any_cast<uint64_t>(_data) << 30;
  return format_hex(modifiedValue);
}

void
device::
query_and_put(QueryRequest qr, boost::property_tree::ptree & pt) const
{
  auto qre = get_query_entry(qr);
  query_and_put(qr, *(qre->pTypeInfo), pt, qre->sPtreeNodeName, qre->string_formatter);
}

void
device::
query_and_put(QueryRequest qr,
              const std::type_info & tinfo,
              boost::property_tree::ptree & pt,
              const std::string& pname,
              FORMAT_STRING_PTR format) const
{
  try {
    if (format == nullptr) {
      std::string errMsg = boost::str(boost::format("Missing data format help function for request: %d") % (uint32_t) qr);
      throw error(errMsg);
    }

    boost::any value;
    query(qr, tinfo, value);

    if (tinfo == typeid(std::vector<std::string>)) {
      boost::property_tree::ptree pt_array;
      for (auto str : boost::any_cast<std::vector<std::string>>(value)) {
        boost::property_tree::ptree pt_item;
        pt_item.put("", format(boost::any(str)));
        pt_array.push_back(std::make_pair("", pt_item));   // Used to make an array of strings
      }
      pt.add_child(pname, pt_array);
    } else {
      pt.put(pname, format(value));
    }
  } catch (const std::exception& e) {
    pt.put(pname + ":error_msg", e.what());
  }
}

void
device::
get_rom_info(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_ROM_VBNV, pt);
  query_and_put(QR_ROM_DDR_BANK_SIZE, pt);
  query_and_put(QR_ROM_DDR_BANK_COUNT_MAX, pt);
  query_and_put(QR_ROM_FPGA_NAME, pt);
}


void
device::
get_xmc_info(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_XMC_VERSION, pt);
  query_and_put(QR_XMC_SERIAL_NUM, pt);
  query_and_put(QR_XMC_MAX_POWER, pt);
  query_and_put(QR_XMC_BMC_VERSION, pt);
}

void
device::
get_platform_info(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_DNA_SERIAL_NUM, pt);
  query_and_put(QR_CLOCK_FREQS, pt);
  query_and_put(QR_IDCODE, pt);
  query_and_put(QR_STATUS_MIG_CALIBRATED, pt);
  query_and_put(QR_STATUS_P2P_ENABLED, pt);
}

void
device::
read_thermal_pcb(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_TEMP_CARD_TOP_FRONT, pt);
  query_and_put(QR_TEMP_CARD_TOP_REAR, pt);
  query_and_put(QR_TEMP_CARD_BOTTOM_FRONT, pt);
}

void
device::
read_thermal_fpga(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_TEMP_FPGA, pt);
}

void
device::
read_fan_info(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_FAN_TRIGGER_CRITICAL_TEMP, pt);
  query_and_put(QR_FAN_FAN_PRESENCE, pt);
  query_and_put(QR_FAN_SPEED_RPM, pt);
}

void
device::
read_thermal_cage(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_CAGE_TEMP_0, pt);
  query_and_put(QR_CAGE_TEMP_1, pt);
  query_and_put(QR_CAGE_TEMP_2, pt);
  query_and_put(QR_CAGE_TEMP_3, pt);
}

void
device::
read_electrical(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_12V_PEX_MILLIVOLTS, pt);
  query_and_put(QR_12V_PEX_MILLIAMPS,  pt);
  query_and_put(QR_12V_AUX_MILLIVOLTS, pt);
  query_and_put(QR_12V_AUX_MILLIAMPS,  pt);

  query_and_put(QR_3V3_PEX_MILLIVOLTS, pt);
  query_and_put(QR_3V3_AUX_MILLIVOLTS, pt);
  query_and_put(QR_DDR_VPP_BOTTOM_MILLIVOLTS, pt);
  query_and_put(QR_DDR_VPP_TOP_MILLIVOLTS, pt);


  query_and_put(QR_5V5_SYSTEM_MILLIVOLTS, pt);
  query_and_put(QR_1V2_VCC_TOP_MILLIVOLTS, pt);
  query_and_put(QR_1V2_VCC_BOTTOM_MILLIVOLTS, pt);
  query_and_put(QR_1V8_MILLIVOLTS, pt);
  query_and_put(QR_0V85_MILLIVOLTS, pt);
  query_and_put(QR_0V9_VCC_MILLIVOLTS, pt);
  query_and_put(QR_12V_SW_MILLIVOLTS, pt);
  query_and_put(QR_MGT_VTT_MILLIVOLTS, pt);
  query_and_put(QR_INT_VCC_MILLIVOLTS, pt);
  query_and_put(QR_INT_VCC_MILLIAMPS, pt);

  query_and_put(QR_3V3_PEX_MILLIAMPS, pt);
  query_and_put(QR_0V85_MILLIAMPS, pt);
  query_and_put(QR_3V3_VCC_MILLIVOLTS, pt);
  query_and_put(QR_HBM_1V2_MILLIVOLTS, pt);
  query_and_put(QR_2V5_VPP_MILLIVOLTS, pt);
  query_and_put(QR_INT_BRAM_VCC_MILLIVOLTS, pt);
}

void
device::
read_power(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_POWER_MICROWATTS, pt);
}


void
device::
read_firewall(boost::property_tree::ptree& pt) const
{
  query_and_put(QR_FIREWALL_DETECT_LEVEL, pt);
  query_and_put(QR_FIREWALL_STATUS, pt);
  query_and_put(QR_FIREWALL_TIME_SEC, pt);
}

} // xrt_core
