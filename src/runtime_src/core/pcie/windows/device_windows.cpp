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
#include "include/xrt.h"
#include <string>
#include <iostream>
#include "boost/format.hpp"
#include <map>
#include <fstream>

#include <setupapi.h>
#include "core/pcie/driver/windows/include/XoclUser_INTF.h"

#pragma warning(disable : 4100 4996)
#pragma comment (lib, "Setupapi.lib")


 //mgmt GUID
DEFINE_GUID(GUID_XILINX_PF_INTERFACE,
	0xd5bf220b, 0xf9c4, 0x415d, 0xbf, 0xac, 0x8, 0x6e, 0xbd, 0x65, 0x3f, 0x8f);


const xrt_core::device_windows::IOCTLEntry & 
xrt_core::device_windows::get_IOCTL_entry( QueryRequest _eQueryRequest) const
{
  // Initialize our lookup table
  static const std::map<QueryRequest, IOCTLEntry> QueryRequestToIOCTLTable =
  {
	{ QR_PCIE_VENDOR,               {pcie,   vendor}},
	{ QR_PCIE_DEVICE,               {pcie,   device}},
	{ QR_PCIE_SUBSYSTEM_VENDOR,     {pcie,   subsystem_vendor}},
	{ QR_PCIE_SUBSYSTEM_ID,         {pcie,   subsystem_device}},
	{ QR_PCIE_LINK_SPEED,           {pcie,   link_speed}},
	{ QR_PCIE_EXPRESS_LANE_WIDTH,   {pcie,   link_width}},
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
xrt_core::device_windows::query_device(uint64_t _deviceID, QueryRequest _eQueryRequest, const std::type_info & _typeInfo, boost::any &_returnValue) const
{
  // Initialize return data to being empty container.
  // Note: CentOS Boost 1.53 doesn't support the clear() method.
  boost::any anyEmpty;
  _returnValue.swap(anyEmpty);

  // Get the sysdev and entry values to call
  const IOCTLEntry & entry = get_IOCTL_entry(_eQueryRequest);

  std::string sErrorMsg;

  if (entry.subdev == 0) {
    sErrorMsg = "IOCTLEntry is initialized with zeros.";
  }

  // Removes compile warnings for unused variables
  _deviceID = _deviceID;
  queryDeviceWithQR(deviceHandle[_deviceID],
	  entry.subdev,
	  entry.variable,
	  _returnValue);

  sErrorMsg = boost::str( boost::format("Error: Unsupported query_device return type: '%s'") % _typeInfo.name());

  if (!sErrorMsg.empty()) {
    throw std::runtime_error(sErrorMsg);
  }
}


void xrt_core::initialize_child_ctor()
{
  xrt_core::device_windows::register_child_ctor(boost::factory<xrt_core::device_windows *>());
}

xrt_core::device_windows::device_windows()
{
	total_devices = get_total_devices();
	for (int _deviceID = 0; _deviceID < total_devices; _deviceID++) {
		xclDeviceHandle m_handle = nullptr;
		m_handle = xclOpen(_deviceID, 0, XCL_INFO);
		if (!m_handle)
			std::cout << "Failed to open device: " << _deviceID;

		deviceHandle.push_back(m_handle);
	}
}

xrt_core::device_windows::~device_windows() {
  // Do nothing
}

uint64_t 
xrt_core::device_windows::get_total_devices() const
{
  // Linux reference code: 
  // return pcidev::get_dev_total();
	SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
	//SP_DEVINFO_DATA DeviceInfoData;

	unsigned int mgmt_count, user_count;

	HDEVINFO hDevInfo;

	//finding number of user_devices
	hDevInfo = SetupDiGetClassDevs(&GUID_DEVINTERFACE_XOCL_USER,
		NULL,
		NULL,
		DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	user_count = 0;
	while (SetupDiEnumDeviceInterfaces(hDevInfo,
		NULL,
		&GUID_DEVINTERFACE_XOCL_USER,
		user_count++,
		&DeviceInterfaceData)
		);

	user_count--;

	if (user_count == 0) {
		std::cout << "No Xilinx U250 devices are present and enabled in the system" << std::endl;
	}
	else {
		//std::cout << "Number of User devices found: " << user_count << std::endl;
	}

	//Finding number of mgmt_devices
	hDevInfo = SetupDiGetClassDevs(&GUID_XILINX_PF_INTERFACE,
		NULL,
		NULL,
		DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	mgmt_count = 0;
	while (SetupDiEnumDeviceInterfaces(hDevInfo,
		NULL,
		&GUID_XILINX_PF_INTERFACE,
		mgmt_count++,
		&DeviceInterfaceData)
		);

	mgmt_count--;

	if (mgmt_count == 0) {
		std::cout << "No Xilinx U250 devices are present and enabled in the system" << std::endl;
	}
	else {
		//std::cout << "Number of management devices found: " << mgmt_count << std::endl;
	}

	return user_count;
}

void 
xrt_core::device_windows::read_device_dma_stats(uint64_t _deviceID, boost::property_tree::ptree &_pt) const
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

int
xrt_core::device_windows::program_device(uint64_t _deviceID, uint64_t region, const std::string &sXclBin) const
{
	//std::cout << ":program_device: " << _deviceID << region << sXclBin << std::endl;

	std::ifstream stream;
	stream.open(sXclBin.c_str());

	if (!stream.is_open()) {
		std::cout << "ERROR: Cannot open " << sXclBin <<
			". Check that it exists and is readable." << std::endl;
		return -ENOENT;
	}

	if (region) {
		std::cout << "ERROR: Not support other than -r 0 " << std::endl;
		return -EINVAL;
	}

	char temp[8];
	stream.read(temp, 8);

	std::cout << temp << std::endl;
	if (std::strncmp(temp, "xclbin0", 8)) {
		if (std::strncmp(temp, "xclbin2", 8))
			return -EINVAL;
	}
	stream.seekg(0, stream.end);
	std::streamoff length = stream.tellg();
	stream.seekg(0, stream.beg);

	char *buffer = new char[length];
	stream.read(buffer, length);
	const xclBin *header = (const xclBin *)buffer;

	int result = xclLockDevice(deviceHandle[_deviceID]);
	if (result == 0)
		result = xclLoadXclBin(deviceHandle[_deviceID], header);
	delete[] buffer;
	int val = xclUnlockDevice(deviceHandle[_deviceID]);

	return val;

}
