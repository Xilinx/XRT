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


#define INITGUID
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

const xrt_core::device_windows::IOCTLEntry &
xrt_core::device_windows::
get_IOCTL_entry( QueryRequest _eQueryRequest) const
{
  // Initialize our lookup table
  static const std::map<QueryRequest, IOCTLEntry> QueryRequestToIOCTLTable =
  {
	{ QR_PCIE_VENDOR,               {pcie,   vendor}},
	{ QR_PCIE_DEVICE,               {pcie,   pcie_device}},
	{ QR_PCIE_SUBSYSTEM_VENDOR,     {pcie,   subsystem_vendor}},
	{ QR_PCIE_SUBSYSTEM_ID,         {pcie,   subsystem_device}},
	{ QR_PCIE_LINK_SPEED,           {pcie,   link_speed}},
	{ QR_PCIE_EXPRESS_LANE_WIDTH,   {pcie,   link_width}},
	{ QR_PCIE_READY_STATUS,         {pcie,   ready}},
	{ QR_DMA_THREADS_RAW,           {dma,  channel_stat_raw_v}},
	{ QR_ROM_VBNV,                  {rom,  VBNV}},
	{ OR_ROM_DDR_BANK_SIZE,         {rom,  ddr_bank_size}},
	{ QR_ROM_DDR_BANK_COUNT_MAX,    {rom,  ddr_bank_count_max}},
	{ QR_ROM_FPGA_NAME,             {rom,  FPGA}},
	{ QR_XMC_VERSION,               {xmc,  version}},
	{ QR_XMC_SERIAL_NUM,            {xmc,  serial_num}},
	{ QR_XMC_MAX_POWER,             {xmc,  max_power}},
	{ QR_XMC_BMC_VERSION,           {xmc,  bmc_ver}},
	{ QR_DNA_SERIAL_NUM,            {dna,  dna_v}},
	{ QR_CLOCK_FREQS,               {icap,  clock_freqs}},
	{ QR_IDCODE,                    {icap,  idcode}},
	{ QR_STATUS_MIG_CALIBRATED,     {pcie,  mig_calibration}},
	{ QR_STATUS_P2P_ENABLED,        {pcie,  p2p_enable}},
	{ QR_TEMP_CARD_TOP_FRONT,       {xmc,  xmc_se98_temp0}},
	{ QR_TEMP_CARD_TOP_REAR,        {xmc,  xmc_se98_temp1}},
	{ QR_TEMP_CARD_BOTTOM_FRONT,    {xmc,  xmc_se98_temp2}},
	{ QR_TEMP_FPGA,                 {xmc,  xmc_fpga_temp}},
	{ QR_FAN_TRIGGER_CRITICAL_TEMP, {xmc,  xmc_fan_temp}},
	{ QR_FAN_FAN_PRESENCE,          {xmc,  fan_presence}},
	{ QR_FAN_SPEED_RPM,             {xmc,  xmc_fan_rpm}},
	{ QR_CAGE_TEMP_0,               {xmc,  xmc_cage_temp0}},
	{ QR_CAGE_TEMP_1,               {xmc,  xmc_cage_temp1}},
	{ QR_CAGE_TEMP_2,               {xmc,  xmc_cage_temp2}},
	{ QR_CAGE_TEMP_3,               {xmc,  xmc_cage_temp3}},
	{ QR_12V_PEX_MILLIVOLTS,        {xmc,  xmc_12v_pex_vol}},
	{ QR_12V_PEX_MILLIAMPS,         {xmc,  xmc_12v_pex_curr}},
	{ QR_12V_AUX_MILLIVOLTS,        {xmc,  xmc_12v_aux_vol}},
	{ QR_12V_AUX_MILLIAMPS,         {xmc,  xmc_12v_aux_curr}},
	{ QR_3V3_PEX_MILLIVOLTS,        {xmc,  xmc_3v3_pex_vol}},
	{ QR_3V3_AUX_MILLIVOLTS,        {xmc,  xmc_3v3_aux_vol}},
	{ QR_DDR_VPP_BOTTOM_MILLIVOLTS, {xmc,  xmc_ddr_vpp_btm}},
	{ QR_DDR_VPP_TOP_MILLIVOLTS,    {xmc,  xmc_ddr_vpp_top}},

	{ QR_5V5_SYSTEM_MILLIVOLTS,     {xmc,  xmc_sys_5v5}},
	{ QR_1V2_VCC_TOP_MILLIVOLTS,    {xmc,  xmc_1v2_top}},
	{ QR_1V2_VCC_BOTTOM_MILLIVOLTS, {xmc,  xmc_vcc1v2_btm}},
	{ QR_1V8_MILLIVOLTS,            {xmc,  xmc_1v8}},
	{ QR_0V85_MILLIVOLTS,           {xmc,  xmc_0v85}},
	{ QR_0V9_VCC_MILLIVOLTS,        {xmc,  xmc_mgt0v9avcc}},
	{ QR_12V_SW_MILLIVOLTS,         {xmc,  xmc_12v_sw}},
	{ QR_MGT_VTT_MILLIVOLTS,        {xmc,  xmc_mgtavtt}},
	{ QR_INT_VCC_MILLIVOLTS,        {xmc,  xmc_vccint_vol}},
	{ QR_INT_VCC_MILLIAMPS,         {xmc,  xmc_vccint_curr}},

	{ QR_3V3_PEX_MILLIAMPS,         {xmc,  xmc_3v3_pex_curr}},
	{ QR_0V85_MILLIAMPS,            {xmc,  xmc_0v85_curr}},
	{ QR_3V3_VCC_MILLIVOLTS,        {xmc,  xmc_3v3_vcc_vol}},
	{ QR_HBM_1V2_MILLIVOLTS,        {xmc,  xmc_hbm_1v2_vol}},
	{ QR_2V5_VPP_MILLIVOLTS,        {xmc,  xmc_vpp2v5_vol}},
	{ QR_INT_BRAM_VCC_MILLIVOLTS,   {xmc,  xmc_vccint_bram_vol}},

	{ QR_FIREWALL_DETECT_LEVEL,     {firewall, detected_level}},
	{ QR_FIREWALL_STATUS,           {firewall, detected_status}},
	{ QR_FIREWALL_TIME_SEC,         {firewall, detected_time}},

	{ QR_POWER_MICROWATTS,          {xmc, xmc_power}}
};
  // Find the translation entry
  std::map<QueryRequest, IOCTLEntry>::const_iterator it = QueryRequestToIOCTLTable.find(_eQueryRequest);

  if (it == QueryRequestToIOCTLTable.end()) {
    std::string errMsg = boost::str( boost::format("The given query request ID (%d) is not supported.") % _eQueryRequest);
    throw std::runtime_error( errMsg);
  }

  return it->second;
}



void
xrt_core::device_windows::
query_device(uint64_t _deviceID, QueryRequest _eQueryRequest, const std::type_info & _typeInfo, boost::any &_returnValue) const
{
  // Initialize return data to being empty container.
  // Note: CentOS Boost 1.53 doesn't support the clear() method.
  boost::any anyEmpty;
  _returnValue.swap(anyEmpty);

  // Get the sysdev and entry values to call
  const IOCTLEntry & entry = get_IOCTL_entry(_eQueryRequest);

  std::string sErrorMsg;

  // Removes compile warnings for unused variables
  _deviceID = _deviceID;

  queryDeviceWithQR(_deviceID,
		  entry.subdev,
		  entry.variable,
		  _returnValue);

  if (!sErrorMsg.empty()) {
    throw std::runtime_error(sErrorMsg);
  }
}

xrt_core::device_core*
xrt_core::
initialize_child_ctor()
{
  static device_windows dw;
  return &dw;
}

xrt_core::device_windows::
device_windows()
{
  // Do nothing
}

xrt_core::device_windows::
~device_windows() {
  // Do nothing
}

std::pair<uint64_t, uint64_t>
xrt_core::device_windows::
get_total_devices() const
{
  uint64_t user_count = xclProbe();

  //TODO: Getting ready status of devices.
  const IOCTLEntry & entry = get_IOCTL_entry(QR_PCIE_READY_STATUS);
  uint64_t ready_count = 0;
  boost::any anyValue;

  for (unsigned int i = 0; i < user_count; i++) {
	queryDeviceWithQR(i, entry.subdev, entry.variable, anyValue);
	bool ready = boost::any_cast<bool>(anyValue);
	if (ready)
	  ready_count++;
  }

  std::cout << "INFO: Found total " << user_count << " card(s), " << ready_count << " are usable.\n";

  return std::make_pair(user_count, ready_count);
}

void
xrt_core::device_windows::
read_device_dma_stats(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
{
  // Removes compiler warnings
  _deviceID = _deviceID;
  _pt = _pt;
  // Linux reference code
//  _deviceID = _deviceID;
//  _pt = _pt;
//  xclDeviceHandle handle = xclOpen(_deviceID, nullptr, XCL_QUIET);
//
//  if (!handle) {
//    // Unable to get a handle
//    return;
//  }
//
//  xclDeviceUsage devstat = { 0 };
//  xclGetUsageInfo(handle, &devstat);
//
//  // Clean up after ourselves
//  xclClose(handle);
//
//  boost::property_tree::ptree ptChannels;
//  for (unsigned index = 0; index < XCL_DEVICE_USAGE_COUNT; ++index) {
//      boost::property_tree::ptree ptDMA;
//      ptDMA.put( "id", std::to_string(index).c_str());
//      ptDMA.put( "h2c", unitConvert(devstat.h2c[index]) );
//      ptDMA.put( "c2h", unitConvert(devstat.c2h[index]) );
//
//      // Create our array of data
//      ptChannels.push_back(std::make_pair("", ptDMA));
//  }
//
//  _pt.add_child( "transfer_metrics.channels", ptChannels);
}

void
xrt_core::device_windows::
scan_devices(bool verbose, bool json) const
{
  std::cout << "TO-DO: scan_devices\n";
  verbose = verbose;
  json = json;
}

void
xrt_core::device_windows::
auto_flash(uint64_t _deviceID, std::string& shell, std::string& id, bool force) const
{
  std::cout << "TO-DO: auto_flash\n";
  _deviceID = _deviceID;
  shell = shell;
  id = id;
  force = force;
}

void
xrt_core::device_windows::
reset_shell(uint64_t _deviceID) const
{
  std::cout << "TO-DO: reset_shell\n";
  _deviceID = _deviceID;
}

void
xrt_core::device_windows::
update_shell(uint64_t _deviceID, std::string flashType, std::string& primary, std::string& secondary) const
{
  std::cout << "TO-DO: update_shell\n";
  _deviceID = _deviceID;
  flashType = flashType;
  primary = primary;
  secondary = secondary;
}

void
xrt_core::device_windows::
update_SC(uint64_t _deviceID, std::string& file) const
{
  std::cout << "TO-DO: update_SC\n";
  _deviceID = _deviceID;
  file = file;
}
DWORD
xrt_core::device_windows::
get_IpLayoutSize(uint64_t _deviceID) const
{
  return shim_getIPLayoutSize(_deviceID);
}

void
xrt_core::device_windows::
get_IpLayout(uint64_t _deviceID, XU_IP_LAYOUT **ipLayout, DWORD size) const
{
  shim_getIPLayout(_deviceID, ipLayout, size);
}

void
xrt_core::device_windows::
get_memTopology(uint64_t _deviceID, XOCL_MEM_TOPOLOGY_INFORMATION *topoInfo) const
{
  shim_getMemTopology(_deviceID, topoInfo);
}

void
xrt_core::device_windows::
get_memRawInfo(uint64_t _deviceID, XOCL_MEM_RAW_INFORMATION *memRaw) const
{
  shim_getMemRawInfo(_deviceID, memRaw);
}