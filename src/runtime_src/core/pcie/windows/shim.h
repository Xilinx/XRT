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

#ifndef _XRT_CORE_PCIE_WINDOWS_SHIM_H
#define _XRT_CORE_PCIE_WINDOWS_SHIM_H

#include "xrt.h"
#include "core/common/xrt_profiling.h"
#include "boost/any.hpp"
#include <boost/property_tree/ptree.hpp>
#include "core/pcie/driver/windows/include/XoclUser_INTF.h"

typedef enum {
	pcie = 0,
	rom,
	icap,
	xmc,
	firewall,
	dma,
	dna
}subdev;

typedef enum {
	VBNV = 0,
	ddr_bank_size,
	ddr_bank_count_max,
	FPGA
}rom_variable;

typedef enum {
	vendor = 0,
	pcie_device,
	subsystem_vendor,
	subsystem_device,
	link_speed,
	link_width,
	mig_calibration,
	p2p_enable,
	ready
}pcie_variable;

typedef enum {
	clock_freqs = 0,
	idcode
}icap_variable;

typedef enum {
	detected_level = 0,
	detected_status,
	detected_time
}firewall_variable;

typedef enum {
	version = 0,
	serial_num,
	max_power,
	bmc_ver,
	xmc_se98_temp0,
	xmc_se98_temp1,
	xmc_se98_temp2,
	xmc_fpga_temp,
	xmc_fan_temp,
	fan_presence,
	xmc_fan_rpm,
	xmc_cage_temp0,
	xmc_cage_temp1,
	xmc_cage_temp2,
	xmc_cage_temp3,
	xmc_12v_pex_vol,
	xmc_12v_pex_curr,
	xmc_12v_aux_vol,
	xmc_12v_aux_curr,
	xmc_3v3_pex_vol,
	xmc_3v3_aux_vol,
	xmc_ddr_vpp_btm,
	xmc_ddr_vpp_top,
	xmc_sys_5v5,
	xmc_1v2_top,
	xmc_vcc1v2_btm,
	xmc_1v8,
	xmc_0v85,
	xmc_mgt0v9avcc,
	xmc_12v_sw,
	xmc_mgtavtt,
	xmc_vccint_vol,
	xmc_vccint_curr,

	xmc_3v3_pex_curr,
	xmc_0v85_curr,
	xmc_3v3_vcc_vol,
	xmc_hbm_1v2_vol,
	xmc_vpp2v5_vol,
	xmc_vccint_bram_vol,
	xmc_power
}xmc_variable;

void qr_rom_info(HANDLE handle, uint64_t variable, boost::any & _returnValue);
void qr_pcie_info(HANDLE handle, uint64_t variable, boost::any & _returnValue);

__declspec(dllexport) void queryDeviceWithQR(uint64_t _deviceID, uint64_t subdev, uint64_t variable, boost::any & _returnValue);

__declspec(dllexport) void shim_getIPLayout(uint64_t _deviceID, XU_IP_LAYOUT **ipLayout, DWORD size);

__declspec(dllexport) DWORD shim_getIPLayoutSize(uint64_t _deviceID);

__declspec(dllexport) void shim_getMemTopology(uint64_t _deviceID, XOCL_MEM_TOPOLOGY_INFORMATION *topoInfo);
__declspec(dllexport) void shim_getMemRawInfo(uint64_t _deviceID, XOCL_MEM_RAW_INFORMATION *memRaw);

namespace xocl { // shared implementation

} // xocl

#endif
