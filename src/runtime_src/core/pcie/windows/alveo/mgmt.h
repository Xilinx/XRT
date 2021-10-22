/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 */
#ifndef XRT_CORE_PCIE_WINDOWS_ALVEO_MGMT_H
#define XRT_CORE_PCIE_WINDOWS_ALVEO_MGMT_H

#include "config.h"
#include "xrt.h"

#include <initguid.h>
#include "core/pcie/driver/windows/alveo/include/XoclMgmt_INTF.h"

struct FeatureRomHeader;

namespace mgmtpf { // shared implementation

unsigned int
probe();

xclDeviceHandle
open(unsigned int);

void
close(xclDeviceHandle hdl);

void
read_bar(xclDeviceHandle hdl, uint64_t offset, void* buf, uint64_t len);

void
write_bar(xclDeviceHandle hdl, uint64_t offset, const void* buf, uint64_t len);

void
get_rom_info(xclDeviceHandle hdl, FeatureRomHeader* value);

void
get_xmc_info(xclDeviceHandle hdl /*, ??? *value*/);

void
get_device_info(xclDeviceHandle hdl, XCLMGMT_IOC_DEVICE_INFO* value);

void
get_dev_info(xclDeviceHandle hdl, XCLMGMT_DEVICE_INFO* value);

void
get_bdf_info(xclDeviceHandle hdl, uint16_t bdf[4]);

void
get_flash_addr(xclDeviceHandle hdl, uint64_t& value);

void
plp_program(xclDeviceHandle hdl, const struct axlf *buffer, bool force);

void
plp_program_status(xclDeviceHandle hdl, uint64_t& plp_status);

void
get_uuids(xclDeviceHandle hdl, XCLMGMT_IOC_UUID_INFO* value);

void
set_data_retention(xclDeviceHandle hdl, uint32_t value);

void
get_data_retention(xclDeviceHandle hdl, uint32_t* value);

void
get_pcie_info(xclDeviceHandle hdl, XCLMGMT_IOC_DEVICE_PCI_INFO* value);

void
get_mailbox_info(xclDeviceHandle hdl, XCLMGMT_IOC_MAILBOX_RECV_INFO* value);

void
get_board_info(xclDeviceHandle hdl, xcl_board_info* value);
} // mgmtpf

#endif
