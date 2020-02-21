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

#include "core/pcie/windows/config.h"
#include "xrt.h"
#include "core/common/xrt_profiling.h"
#include "core/pcie/driver/windows/include/XoclUser_INTF.h"

struct FeatureRomHeader;

namespace userpf {

XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_rom_info(xclDeviceHandle hdl, FeatureRomHeader* value);


XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_device_info(xclDeviceHandle hdl, XOCL_DEVICE_INFORMATION* value);

/**
 * get_mem_topology() - Get xclbin mem topology from driver
 *
 * @hdl: device handle
 * @buffer: buffer to hold the mem topology section, ignored if nullptr
 * @size: size of buffer
 * @size_ret: returns actual size in bytes required for buffer, ignored if nullptr
 */
XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_mem_topology(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret);

/**
 * get_ip_layout() - Get xclbin ip layout  from driver
 *
 * @hdl: device handle
 * @buffer: buffer to hold the iplayout section, ignored if nullptr
 * @size: size of buffer
 * @size_ret: returns actual size in bytes required for buffer, ignored if nullptr
 */
XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_ip_layout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret);

/**
 * get_debug_ip_layout() - Get xclbin debug ip layout  from driver
 *
 * @hdl: device handle
 * @buffer: buffer to hold the debug iplayout section, ignored if nullptr
 * @size: size of buffer
 * @size_ret: returns actual size in bytes required for buffer, ignored if nullptr
 */
XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_debug_ip_layout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret);


XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_bdf_info(xclDeviceHandle hdl, uint16_t bdf[3]);

XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_sensor_info(xclDeviceHandle hdl, xcl_sensor* value);

XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_icap_info(xclDeviceHandle hdl, xcl_hwicap* value);

XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_board_info(xclDeviceHandle hdl, xcl_board_info* value);

XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_mig_ecc_info(xclDeviceHandle hdl, xcl_mig_ecc* value);

XRT_CORE_PCIE_WINDOWS_EXPORT
void
get_firewall_info(xclDeviceHandle hdl, xcl_firewall* value);

} // userpf


#endif
