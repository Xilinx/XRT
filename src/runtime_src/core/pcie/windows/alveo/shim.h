/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
 */
#ifndef XRT_CORE_PCIE_WINDOWS_ALVEO_SHIM_H
#define XRT_CORE_PCIE_WINDOWS_ALVEO_SHIM_H

#include "config.h"
#include "xrt.h"
#include "core/pcie/driver/windows/alveo/include/XoclUser_INTF.h"

struct FeatureRomHeader;

namespace userpf {

void
get_rom_info(xclDeviceHandle hdl, FeatureRomHeader* value);

void
get_device_info(xclDeviceHandle hdl, XOCL_DEVICE_INFORMATION* value);

// get_mem_topology() - Get xclbin mem topology from driver
//
// @hdl: device handle
// @buffer: buffer to hold the mem topology section, ignored if nullptr
// @size: size of buffer
// @size_ret: returns actual size in bytes required for buffer, ignored if nullptr
void
get_mem_topology(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret);

// get_ip_layout() - Get xclbin ip layout  from driver
//
// @hdl: device handle
// @buffer: buffer to hold the iplayout section, ignored if nullptr
// @size: size of buffer
// @size_ret: returns actual size in bytes required for buffer, ignored if nullptr
void
get_ip_layout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret);

// get_debug_ip_layout() - Get xclbin debug ip layout  from driver
//
// @hdl: device handle
// @buffer: buffer to hold the debug iplayout section, ignored if nullptr
// @size: size of buffer
// @size_ret: returns actual size in bytes required for buffer, ignored if nullptr
void
get_debug_ip_layout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret);

void
get_bdf_info(xclDeviceHandle hdl, uint16_t bdf[3]);

void
get_mailbox_info(xclDeviceHandle hdl, xcl_mailbox* value);

void
get_sensor_info(xclDeviceHandle hdl, xcl_sensor* value);

void
get_icap_info(xclDeviceHandle hdl, xcl_pr_region* value);

void
get_board_info(xclDeviceHandle hdl, xcl_board_info* value);

void
get_mig_ecc_info(xclDeviceHandle hdl, xcl_mig_ecc* value);

void
get_firewall_info(xclDeviceHandle hdl, xcl_firewall* value);

void
get_kds_custat(xclDeviceHandle hdl, char* buffer, DWORD size, int* size_ret);

void
get_temp_by_mem_topology(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret);

void
get_group_mem_topology(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret);

void
get_memstat(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret, bool raw);
} // userpf


#endif
