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

// For now device_core.cpp is delivered with core library (libxrt_core), see
// for example core/pcie/windows/CMakeLists.txt.  To prevent compilation of
// this file from importing symbols from libxrt_core we define this source
// file to instead export with same macro as used in libxrt_core.
#define XCL_DRIVER_DLL_EXPORT

#include "device_core.h"
#include "error.h"
#include "utils.h"
#include "include/xrt.h"
#include "boost/format.hpp"
#include <string>
#include <iostream>
#include <fstream>

namespace xrt_core {

std::map<device_core::QueryRequest, device_core::QueryRequestEntry> device_core::m_QueryTable =
{
  { QR_PCIE_VENDOR,               { "QR_PCIE_VENDOR",               "vendor",           &typeid(std::string), device_core::format_primative }},
  { QR_PCIE_DEVICE,               { "QR_PCIE_DEVICE",               "device",           &typeid(std::string), device_core::format_primative }},
  { QR_PCIE_SUBSYSTEM_VENDOR,     { "QR_PCIE_SUBSYSTEM_VENDOR",     "subsystem_vendor", &typeid(std::string), device_core::format_primative }},
  { QR_PCIE_SUBSYSTEM_ID,         { "QR_PCIE_SUBSYSTEM_ID",         "subsystem_id",     &typeid(std::string), device_core::format_primative }},
  { QR_PCIE_LINK_SPEED,           { "QR_PCIE_LINK_SPEED",           "link_speed",       &typeid(uint64_t),    device_core::format_primative }},
  { QR_PCIE_EXPRESS_LANE_WIDTH,   { "QR_PCIE_EXPRESS_LANE_WIDTH",   "width",            &typeid(uint64_t),    device_core::format_primative }},

  { QR_ROM_VBNV,                  { "QR_ROM_VBNV",                  "vbnv",             &typeid(std::string), device_core::format_primative }},
  { OR_ROM_DDR_BANK_SIZE,         { "OR_ROM_DDR_BANK_SIZE",         "ddr_size_bytes",   &typeid(uint64_t),    device_core::format_hex_base2_shiftup30 }},
  { QR_ROM_DDR_BANK_COUNT_MAX,    { "QR_ROM_DDR_BANK_COUNT_MAX",    "widdr_countdth",   &typeid(uint64_t),    device_core::format_primative }},
  { QR_ROM_FPGA_NAME,             { "QR_ROM_FPGA_NAME",             "fpga_name",        &typeid(std::string), device_core::format_primative }},
  { QR_DMA_THREADS_RAW,           { "QR_DMA_THREADS_RAW",           "dma_threads",      &typeid(std::vector<std::string>),  device_core::format_primative }},

  { QR_XMC_VERSION,               { "QR_XMC_VERSION",               "xmc_version",      &typeid(std::string),  device_core::format_primative }},
  { QR_XMC_SERIAL_NUM,            { "QR_XMC_SERIAL_NUM",            "serial_number",    &typeid(std::string),  device_core::format_primative }},
  { QR_XMC_MAX_POWER,             { "QR_XMC_MAX_POWER",             "max_power",        &typeid(std::string),  device_core::format_primative }},
  { QR_XMC_BMC_VERSION,           { "QR_XMC_BMC_VERSION",           "satellite_controller_version", &typeid(std::string),  device_core::format_primative }},

  { QR_DNA_SERIAL_NUM,            { "QR_DNA_SERIAL_NUM",            "dna",              &typeid(std::string),  device_core::format_primative }},
  { QR_CLOCK_FREQS,               { "QR_CLOCK_FREQS",               "clocks",           &typeid(std::vector<std::string>),  device_core::format_primative }},
  { QR_IDCODE,                    { "QR_IDCODE",                    "idcode",           &typeid(std::string),  device_core::format_primative }},

  { QR_STATUS_MIG_CALIBRATED,     { "QR_STATUS_MIG_CALIBRATED",     "mig_calibrated",   &typeid(bool),  device_core::format_primative }},
  { QR_STATUS_P2P_ENABLED,        { "QR_STATUS_P2P_ENABLED",        "p2p_enabled",      &typeid(bool),  device_core::format_primative }},

  { QR_TEMP_CARD_TOP_FRONT,       { "QR_TEMP_CARD_TOP_FRONT",       "temp_top_front_C",    &typeid(uint64_t),  device_core::format_primative }},
  { QR_TEMP_CARD_TOP_REAR,        { "QR_TEMP_CARD_TOP_REAR",        "temp_top_rear_C",     &typeid(uint64_t),  device_core::format_primative }},
  { QR_TEMP_CARD_BOTTOM_FRONT,    { "QR_TEMP_CARD_BOTTOM_FRONT",    "temp_bottom_front_C", &typeid(uint64_t),  device_core::format_primative }},

  { QR_TEMP_FPGA,                 { "QR_TEMP_FPGA",                 "temp_C",           &typeid(uint64_t),  device_core::format_primative }},

  { QR_FAN_TRIGGER_CRITICAL_TEMP, { "QR_FAN_TRIGGER_CRITICAL_TEMP", "temp_trigger_critical_C",  &typeid(uint64_t),  device_core::format_primative }},
  { QR_FAN_FAN_PRESENCE,          { "QR_FAN_FAN_PRESENCE",          "fan_presence",             &typeid(std::string),  device_core::format_primative }},
  { QR_FAN_SPEED_RPM,             { "QR_FAN_SPEED_RPM",             "fan_speed_rpm",            &typeid(uint64_t),  device_core::format_primative }},

  { QR_CAGE_TEMP_0,               { "QR_CAGE_TEMP_0",               "temp0_C",          &typeid(uint64_t),  device_core::format_primative }},
  { QR_CAGE_TEMP_1,               { "QR_CAGE_TEMP_1",               "temp1_C",          &typeid(uint64_t),  device_core::format_primative }},
  { QR_CAGE_TEMP_2,               { "QR_CAGE_TEMP_2",               "temp2_C",          &typeid(uint64_t),  device_core::format_primative }},
  { QR_CAGE_TEMP_3,               { "QR_CAGE_TEMP_3",               "temp3_C",          &typeid(uint64_t),  device_core::format_primative }},

  { QR_12V_PEX_MILLIVOLTS,        { "QR_12V_PEX_MILLIVOLTS",        "12v_pex.voltage",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_12V_PEX_MILLIAMPS,         { "QR_12V_PEX_MILLIAMPS",         "12v_pex.current",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_12V_AUX_MILLIVOLTS,        { "QR_12V_AUX_MILLIVOLTS",        "12v_aux.voltage",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_12V_AUX_MILLIAMPS,         { "QR_12V_AUX_MILLIAMPS",         "12v_aux.current",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},

  { QR_3V3_PEX_MILLIVOLTS,        { "QR_3V3_PEX_MILLIVOLTS",        "3v3_pex.voltaget", &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_3V3_AUX_MILLIVOLTS,        { "QR_3V3_AUX_MILLIVOLTS",        "3v3_aux.voltage",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},

  { QR_DDR_VPP_BOTTOM_MILLIVOLTS, { "QR_DDR_VPP_BOTTOM_MILLIVOLTS", "ddr_vpp_bottom.voltage", &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_DDR_VPP_TOP_MILLIVOLTS,    { "QR_DDR_VPP_TOP_MILLIVOLTS",    "ddr_vpp_top.voltage",    &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},

  { QR_5V5_SYSTEM_MILLIVOLTS,     { "QR_5V5_SYSTEM_MILLIVOLTS",    "sys_5v5.voltage",   &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},

  { QR_1V2_VCC_TOP_MILLIVOLTS,    { "QR_1V2_VCC_TOP_MILLIVOLTS",    "1v2_top.voltage",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_1V2_VCC_BOTTOM_MILLIVOLTS, { "QR_1V2_VCC_BOTTOM_MILLIVOLTS", "1v2_btm.voltage",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},

  { QR_0V85_MILLIVOLTS,           { "QR_0V85_MILLIVOLTS",           "0v85.voltage",     &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},

  { QR_1V8_MILLIVOLTS,            { "QR_1V8_MILLIVOLTS",            "1v8.voltage",      &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_0V9_VCC_MILLIVOLTS,        { "QR_0V9_VCC_MILLIVOLTS",        "mgt_0v9.voltage",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_12V_SW_MILLIVOLTS,         { "QR_12V_SW_MILLIVOLTS",         "12v_sw.voltage",   &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},

  { QR_MGT_VTT_MILLIVOLTS,        { "QR_MGT_VTT_MILLIVOLTS",        "mgt_vtt.voltage",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_INT_VCC_MILLIVOLTS,        { "QR_INT_VCC_MILLIVOLTS",        "vccint.voltage",   &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_INT_VCC_MILLIAMPS,         { "QR_INT_VCC_MILLIAMPS",         "vccint.current",   &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},

  { QR_3V3_PEX_MILLIAMPS,         { "QR_3V3_PEX_MILLIAMPS",         "3v3_pex.current",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_0V85_MILLIAMPS,            { "QR_0V85_MILLIAMPS",            "0v85.current",     &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_3V3_VCC_MILLIVOLTS,        { "QR_3V3_VCC_MILLIVOLTS",        "vcc3v3.voltage",   &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_HBM_1V2_MILLIVOLTS,        { "QR_HBM_1V2_MILLIVOLTS",        "hbm_1v2.voltage",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_2V5_VPP_MILLIVOLTS,        { "QR_2V5_VPP_MILLIVOLTS",        "vpp2v5.voltage",   &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},
  { QR_INT_BRAM_VCC_MILLIVOLTS,   { "QR_INT_BRAM_VCC_MILLIVOLTS",   "vccint_bram.voltage",  &typeid(uint64_t),  device_core::format_base10_shiftdown3 }},

  { QR_FIREWALL_DETECT_LEVEL,     { "QR_FIREWALL_DETECT_LEVEL",     "level",            &typeid(uint64_t),  device_core::format_primative }},
  { QR_FIREWALL_STATUS,           { "QR_FIREWALL_STATUS",           "status",           &typeid(uint64_t),  device_core::format_hex }},
  { QR_FIREWALL_TIME_SEC,         { "QR_FIREWALL_TIME_SEC",         "time_sec",         &typeid(uint64_t),  device_core::format_primative }},

  { QR_POWER_MICROWATTS,          { "QR_POWER_MICROWATTS",          "power_watts",      &typeid(uint64_t),  device_core::format_base10_shiftdown6 }}
};


const device_core::QueryRequestEntry *
device_core::get_query_entry(QueryRequest _eQueryRequest) const
{
  std::map<QueryRequest, QueryRequestEntry>::const_iterator it = m_QueryTable.find(_eQueryRequest);

  if (it == m_QueryTable.end()) {
    std::string errMsg = boost::str( boost::format("The given query request ID (%d) was dont found.") % _eQueryRequest);
    throw error(errMsg);
  }

  return &m_QueryTable[_eQueryRequest];
}

device_core::device_core()
{
  // Do nothing
}

device_core::~device_core()
{
  // Do nothing
}


device_core* device_core_child_ctor(); // forward declaration

const device_core &
device_core::instance()
{
  //  device_core* device_core_ctor(); // fwd decl, fails to scope properly on windows
  static device_core *pSingleton = device_core_child_ctor();
  return *pSingleton;
}

device_core::device
device_core::get_device(uint64_t _deviceID) const
{
  static bool device_message = true;
  if (device_message) {
    device_message = false;
    auto devices = get_total_devices();
    std::cout << "INFO: Found total " << devices.first << " card(s), "
              << devices.second << " are usable.\n";
  }

  return device(_deviceID);
}

std::string
device_core::format_primative(const boost::any &_data)
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
device_core::format_hex(const boost::any & _data)
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
device_core::format_base10_shiftdown3(const boost::any &_data)
{
  if (_data.type() != typeid(uint64_t)) {
    return format_primative(_data);
  }

  double value = (double) boost::any_cast<uint64_t>(_data);
  value /= (double) 1000.0;    // Shift down 3
  return to_string(value, 3 /*precision*/ );
}

std::string
device_core::format_base10_shiftdown6(const boost::any &_data)
{
  if (_data.type() != typeid(uint64_t)) {
    return format_primative(_data);
  }

  double value = (double) boost::any_cast<uint64_t>(_data);
  value /= (double) 1000000.0;    // Shift down 3
  return to_string(value, 6 /*precision*/ );
}

std::string
device_core::format_hex_base2_shiftup30(const boost::any &_data)
{
  if (_data.type() != typeid(uint64_t)) {
    return format_primative(_data);
  }

  boost::any modifiedValue = boost::any_cast<uint64_t>(_data) << 30;
  return format_hex(modifiedValue);
}



void
device_core::query_device_and_put(uint64_t _deviceID,
                                            QueryRequest _eQueryRequest,
                                            boost::property_tree::ptree & _pt) const
{
  const QueryRequestEntry *pQREntry = get_query_entry(_eQueryRequest);
  if (pQREntry == nullptr) {
    std::string errMsg = boost::str(boost::format("Missing query request entry for ID:  %d") % (uint32_t) _eQueryRequest);
    throw error(errMsg);
  }

  query_device_and_put(_deviceID, _eQueryRequest, *(pQREntry->pTypeInfo), _pt, pQREntry->sPtreeNodeName, pQREntry->string_formatter);
}

void
device_core::query_device_and_put(uint64_t _deviceID,
                                            QueryRequest _eQueryRequest,
                                            const std::type_info & _typeInfo,
                                            boost::property_tree::ptree & _pt,
                                            const std::string &_sPropertyName,
                                            FORMAT_STRING_PTR stringFormat) const
{
  try {
    if (stringFormat == nullptr) {
      std::string errMsg = boost::str(boost::format("Missing data format help function for request: %d") % (uint32_t) _eQueryRequest);
      throw error(errMsg);
    }

    boost::any anyValue;
    query_device(_deviceID, _eQueryRequest, _typeInfo, anyValue);

    if (_typeInfo == typeid(std::vector<std::string>)) {
      boost::property_tree::ptree ptArray;
      for (auto aString : boost::any_cast<std::vector<std::string>>(anyValue)) {
        boost::property_tree::ptree ptItem;
        ptItem.put("", stringFormat(boost::any(aString)));
        ptArray.push_back(std::make_pair("", ptItem));   // Used to make an array of strings
      }
      _pt.add_child(_sPropertyName, ptArray);
    } else {
      _pt.put(_sPropertyName, stringFormat(anyValue));
    }
  } catch (const std::exception& e) {
    _pt.put(_sPropertyName + ":error_msg", e.what());
  }
}

void
device_core::get_device_rom_info(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  query_device_and_put(_deviceID, QR_ROM_VBNV, _pt);
  query_device_and_put(_deviceID, OR_ROM_DDR_BANK_SIZE, _pt);
  query_device_and_put(_deviceID, QR_ROM_DDR_BANK_COUNT_MAX, _pt);
  query_device_and_put(_deviceID, QR_ROM_FPGA_NAME, _pt);
}


void
device_core::get_device_xmc_info(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{

  query_device_and_put(_deviceID, QR_XMC_VERSION, _pt);
  query_device_and_put(_deviceID, QR_XMC_SERIAL_NUM, _pt);
  query_device_and_put(_deviceID, QR_XMC_MAX_POWER, _pt);
  query_device_and_put(_deviceID, QR_XMC_BMC_VERSION, _pt);
}

void
device_core::get_device_platform_info(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  query_device_and_put(_deviceID, QR_DNA_SERIAL_NUM, _pt);
  query_device_and_put(_deviceID, QR_CLOCK_FREQS, _pt);
  query_device_and_put(_deviceID, QR_IDCODE, _pt);
  query_device_and_put(_deviceID, QR_STATUS_MIG_CALIBRATED, _pt);
  query_device_and_put(_deviceID, QR_STATUS_P2P_ENABLED, _pt);
}

void
device_core::read_device_thermal_pcb(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{

  query_device_and_put(_deviceID, QR_TEMP_CARD_TOP_FRONT, _pt);
  query_device_and_put(_deviceID, QR_TEMP_CARD_TOP_REAR, _pt);
  query_device_and_put(_deviceID, QR_TEMP_CARD_BOTTOM_FRONT, _pt);
}

void
device_core::read_device_thermal_fpga(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  query_device_and_put(_deviceID, QR_TEMP_FPGA, _pt);
}

void
device_core::read_device_fan_info(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  query_device_and_put(_deviceID, QR_FAN_TRIGGER_CRITICAL_TEMP, _pt);
  query_device_and_put(_deviceID, QR_FAN_FAN_PRESENCE, _pt);
  query_device_and_put(_deviceID, QR_FAN_SPEED_RPM, _pt);
}

void
device_core::read_device_thermal_cage(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  query_device_and_put(_deviceID, QR_CAGE_TEMP_0, _pt);
  query_device_and_put(_deviceID, QR_CAGE_TEMP_1, _pt);
  query_device_and_put(_deviceID, QR_CAGE_TEMP_2, _pt);
  query_device_and_put(_deviceID, QR_CAGE_TEMP_3, _pt);
}



void
device_core::read_device_electrical(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  query_device_and_put(_deviceID, QR_12V_PEX_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_12V_PEX_MILLIAMPS,  _pt);
  query_device_and_put(_deviceID, QR_12V_AUX_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_12V_AUX_MILLIAMPS,  _pt);

  query_device_and_put(_deviceID, QR_3V3_PEX_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_3V3_AUX_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_DDR_VPP_BOTTOM_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_DDR_VPP_TOP_MILLIVOLTS, _pt);


  query_device_and_put(_deviceID, QR_5V5_SYSTEM_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_1V2_VCC_TOP_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_1V2_VCC_BOTTOM_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_1V8_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_0V85_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_0V9_VCC_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_12V_SW_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_MGT_VTT_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_INT_VCC_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_INT_VCC_MILLIAMPS, _pt);

  query_device_and_put(_deviceID, QR_3V3_PEX_MILLIAMPS, _pt);
  query_device_and_put(_deviceID, QR_0V85_MILLIAMPS, _pt);
  query_device_and_put(_deviceID, QR_3V3_VCC_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_HBM_1V2_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_2V5_VPP_MILLIVOLTS, _pt);
  query_device_and_put(_deviceID, QR_INT_BRAM_VCC_MILLIVOLTS, _pt);
}

void
device_core::read_device_power(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  query_device_and_put(_deviceID, QR_POWER_MICROWATTS, _pt);
}


void
device_core::read_device_firewall(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  query_device_and_put(_deviceID, QR_FIREWALL_DETECT_LEVEL, _pt);
  query_device_and_put(_deviceID, QR_FIREWALL_STATUS, _pt);
  query_device_and_put(_deviceID, QR_FIREWALL_TIME_SEC, _pt);
}

} // xrt_core
