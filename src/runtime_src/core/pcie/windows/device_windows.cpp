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

#include "device_windows.h"
#include "common/utils.h"
#include "xrt.h"
#include "boost/format.hpp"
#include <string>
#include <iostream>
#include <map>

#pragma warning(disable : 4100 4996)

namespace xrt_core {

const device_windows::IOCTLEntry &
device_windows::
get_IOCTL_entry(QueryRequest qr) const
{
  // Initialize our lookup table
  static const std::map<QueryRequest, IOCTLEntry> QueryRequestToIOCTLTable =
  {
    { QR_PCIE_VENDOR,               { 0 }},
    { QR_PCIE_DEVICE,               { 0 }},
    { QR_PCIE_SUBSYSTEM_VENDOR,     { 0 }},
    { QR_PCIE_SUBSYSTEM_ID,         { 0 }},
    { QR_PCIE_LINK_SPEED,           { 0 }},
    { QR_PCIE_EXPRESS_LANE_WIDTH,   { 0 }},
    { QR_DMA_THREADS_RAW,           { 0 }},
    { QR_ROM_VBNV,                  { 0 }},
    { OR_ROM_DDR_BANK_SIZE,         { 0 }},
    { QR_ROM_DDR_BANK_COUNT_MAX,    { 0 }},
    { QR_ROM_FPGA_NAME,             { 0 }},
    { QR_ROM_RAW,                   { 0 }},
    { QR_ROM_UUID,                  { 0 }},
    { QR_XMC_VERSION,               { 0 }},
    { QR_XMC_SERIAL_NUM,            { 0 }},
    { QR_XMC_MAX_POWER,             { 0 }},
    { QR_XMC_BMC_VERSION,           { 0 }},
    { QR_XMC_STATUS,                { 0 }},
    { QR_XMC_REG_BASE,              { 0 }},
    { QR_DNA_SERIAL_NUM,            { 0 }},
    { QR_CLOCK_FREQS,               { 0 }},
    { QR_IDCODE,                    { 0 }},
    { QR_STATUS_MIG_CALIBRATED,     { 0 }},
    { QR_STATUS_P2P_ENABLED,        { 0 }},
    { QR_TEMP_CARD_TOP_FRONT,       { 0 }},
    { QR_TEMP_CARD_TOP_REAR,        { 0 }},
    { QR_TEMP_CARD_BOTTOM_FRONT,    { 0 }},
    { QR_TEMP_FPGA,                 { 0 }},
    { QR_FAN_TRIGGER_CRITICAL_TEMP, { 0 }},
    { QR_FAN_FAN_PRESENCE,          { 0 }},
    { QR_FAN_SPEED_RPM,             { 0 }},
    { QR_CAGE_TEMP_0,               { 0 }},
    { QR_CAGE_TEMP_1,               { 0 }},
    { QR_CAGE_TEMP_2,               { 0 }},
    { QR_CAGE_TEMP_3,               { 0 }},
    { QR_12V_PEX_MILLIVOLTS,        { 0 }},
    { QR_12V_PEX_MILLIAMPS,         { 0 }},
    { QR_12V_AUX_MILLIVOLTS,        { 0 }},
    { QR_12V_AUX_MILLIAMPS,         { 0 }},
    { QR_3V3_PEX_MILLIVOLTS,        { 0 }},
    { QR_3V3_AUX_MILLIVOLTS,        { 0 }},
    { QR_DDR_VPP_BOTTOM_MILLIVOLTS, { 0 }},
    { QR_DDR_VPP_TOP_MILLIVOLTS,    { 0 }},

    { QR_5V5_SYSTEM_MILLIVOLTS,     { 0 }},
    { QR_1V2_VCC_TOP_MILLIVOLTS,    { 0 }},
    { QR_1V2_VCC_BOTTOM_MILLIVOLTS, { 0 }},
    { QR_1V8_MILLIVOLTS,            { 0 }},
    { QR_0V85_MILLIVOLTS,           { 0 }},
    { QR_0V9_VCC_MILLIVOLTS,        { 0 }},
    { QR_12V_SW_MILLIVOLTS,         { 0 }},
    { QR_MGT_VTT_MILLIVOLTS,        { 0 }},
    { QR_INT_VCC_MILLIVOLTS,        { 0 }},
    { QR_INT_VCC_MILLIAMPS,         { 0 }},

    { QR_3V3_PEX_MILLIAMPS,         { 0 }},
    { QR_0V85_MILLIAMPS,            { 0 }},
    { QR_3V3_VCC_MILLIVOLTS,        { 0 }},
    { QR_HBM_1V2_MILLIVOLTS,        { 0 }},
    { QR_2V5_VPP_MILLIVOLTS,        { 0 }},
    { QR_INT_BRAM_VCC_MILLIVOLTS,   { 0 }},

    { QR_FIREWALL_DETECT_LEVEL,     { 0 }},
    { QR_FIREWALL_STATUS,           { 0 }},
    { QR_FIREWALL_TIME_SEC,         { 0 }},

    { QR_POWER_MICROWATTS,          { 0 }},

    { QR_FLASH_BAR_OFFSET,          { 0 }},
    { QR_IS_MFG,                    { 0 }},
    { QR_F_FLASH_TYPE,              { 0 }},
    { QR_FLASH_TYPE,                { 0 }},
  };
  // Find the translation entry
  std::map<QueryRequest, IOCTLEntry>::const_iterator it = QueryRequestToIOCTLTable.find(qr);

  if (it == QueryRequestToIOCTLTable.end()) {
    std::string errMsg = boost::str( boost::format("The given query request ID (%d) is not supported.") % qr);
    throw no_such_query(qr, errMsg);
  }

  return it->second;
}

void
device_windows::
query(QueryRequest qr, const std::type_info & _typeInfo, boost::any& value) const
{
  // Initialize return data to being empty container.
  // Note: CentOS Boost 1.53 doesn't support the clear() method.
  boost::any anyEmpty;
  value.swap(anyEmpty);

  // Get the sysdev and entry values to call
  const IOCTLEntry & entry = get_IOCTL_entry(qr);

  std::string sErrorMsg;

  if (entry.IOCTLValue == 0) {
    sErrorMsg = "IOCTLEntry is initialized with zeros.";
  }

  // Reference linux code:
//  if (_typeInfo == typeid(std::string)) {
//    // -- Typeid: std::string --
//    _returnValue = std::string("");
//    std::string *stringValue = boost::any_cast<std::string>(&_returnValue);
//    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, *stringValue);
//
//  } else if (_typeInfo == typeid(uint64_t)) {
//    // -- Typeid: uint64_t --
//    _returnValue = (uint64_t) -1;
//    std::vector<uint64_t> uint64Vector;
//    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, uint64Vector);
//    if (!uint64Vector.empty()) {
//      _returnValue = uint64Vector[0];
//    }
//
//  } else if (_typeInfo == typeid(bool)) {
//    // -- Typeid: bool --
//    _returnValue = (bool) 0;
//    std::vector<uint64_t> uint64Vector;
//    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, uint64Vector);
//    if (!uint64Vector.empty()) {
//      _returnValue = (bool) uint64Vector[0];
//    }
//
//  } else if (_typeInfo == typeid(std::vector<std::string>)) {
//    // -- Typeid: std::vector<std::string>
//    _returnValue = std::vector<std::string>();
//    std::vector<std::string> *stringVector = boost::any_cast<std::vector<std::string>>(&_returnValue);
//    pcidev::get_dev(_deviceID)->sysfs_get( entry.sSubDevice, entry.sEntry, sErrorMsg, *stringVector);
//
//  } else {
//  }

  sErrorMsg = boost::str( boost::format("Error: Unsupported query_device return type: '%s'") % _typeInfo.name());

  if (!sErrorMsg.empty()) {
    throw std::runtime_error(sErrorMsg);
  }
}

device_windows::
device_windows(id_type device_id, bool user)
  : device_pcie(device_id, user)
{
}

void
device_windows::
read_dma_stats(boost::property_tree::ptree& pt) const
{
}

void
device_windows::
read(uint64_t addr, void* buf, uint64_t len) const
{
}

void
device_windows::
write(uint64_t addr, const void* buf, uint64_t len) const
{
}

} // xrt_core
