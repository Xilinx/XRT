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
#include "shim.h"
#include "boost/format.hpp"
#include <string>
#include <iostream>
#include <map>
#include <fstream>
#include <setupapi.h>

#pragma warning(disable : 4100 4996)

namespace xrt_core {

const device_windows::IOCTLEntry &
device_windows::
get_IOCTL_entry(QueryRequest qr) const
{
  // Initialize our lookup table
  static const std::map<QueryRequest, IOCTLEntry> QueryRequestToIOCTLTable =
  {
    { QR_PCIE_VENDOR,               { IOCTL_XOCL_STAT,   XoclStatDevice }},
    { QR_PCIE_DEVICE,               { IOCTL_XOCL_STAT,   XoclStatDevice }},
    { QR_PCIE_SUBSYSTEM_VENDOR,     { IOCTL_XOCL_STAT,   XoclStatDevice }},
    { QR_PCIE_SUBSYSTEM_ID,         { IOCTL_XOCL_STAT,   XoclStatDevice }},
    { QR_PCIE_LINK_SPEED,           { IOCTL_XOCL_STAT,   XoclStatDevice }},
    { QR_PCIE_EXPRESS_LANE_WIDTH,   { IOCTL_XOCL_STAT,   XoclStatDevice }},
    { QR_PCIE_READY_STATUS,         { IOCTL_XOCL_STAT,   XoclStatDevice }},
    { QR_DMA_THREADS_RAW,           { 0 }},
    { QR_ROM_VBNV,                  { IOCTL_XOCL_STAT,   XoclStatRomInfo }},
    { QR_ROM_DDR_BANK_SIZE,         { IOCTL_XOCL_STAT,   XoclStatRomInfo }},
    { QR_ROM_DDR_BANK_COUNT_MAX,    { IOCTL_XOCL_STAT,   XoclStatRomInfo }},
    { QR_ROM_FPGA_NAME,             { IOCTL_XOCL_STAT,   XoclStatRomInfo }},
    { QR_ROM_TIME_SINCE_EPOCH,      { IOCTL_XOCL_STAT,   XoclStatRomInfo }},
    { QR_XMC_VERSION,               { 0 }},
    { QR_XMC_SERIAL_NUM,            { 0 }},
    { QR_XMC_MAX_POWER,             { 0 }},
    { QR_XMC_BMC_VERSION,           { 0 }},
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

    { QR_POWER_MICROWATTS,          { 0 }}
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

  auto device_id = get_device_id();

  // Get the sysdev and entry values to call
  const IOCTLEntry & entry = get_IOCTL_entry(qr);

  std::string sErrorMsg;

  if (entry.IOCTLValue == 0) {
    sErrorMsg = "IOCTLEntry is initialized with zeros.";
  } else {
    queryDeviceWithQR(device_id, value, qr, _typeInfo, entry.statClass);
  }

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
auto_flash(const std::string& shell, const std::string& id, bool force) const
{
  std::cout << "TO-DO: auto_flash\n";
}

void
device_windows::
reset_shell() const
{
  std::cout << "TO-DO: reset_shell\n";
}

void
device_windows::
update_shell(const std::string& flashType, const std::string& primary, const std::string& secondary) const
{
  std::cout << "TO-DO: update_shell\n";
}

void
device_windows::
update_SC(const std::string& file) const
{
  std::cout << "TO-DO: update_SC\n";
}

unsigned long
device_windows::
get_ip_layoutsize(uint64_t _deviceID) const
{
  return shim_get_ip_layoutsize(_deviceID);
}

void
device_windows::
get_ip_layout(uint64_t _deviceID, struct ip_layout **ipLayout, unsigned long size) const
{
  shim_get_ip_layout(_deviceID, ipLayout, size);
}

unsigned long
device_windows::
get_mem_topology(uint64_t _deviceID, struct mem_topology *topoInfo) const
{
  return shim_get_mem_topology(_deviceID, topoInfo);
}

unsigned long
device_windows::
get_mem_rawinfo(uint64_t _deviceID, struct mem_raw_info *memRaw) const
{
  return shim_get_mem_rawinfo(_deviceID, memRaw);
}

} // xrt_core
