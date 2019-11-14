/*
 * Copyright (C) 2015-2019, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) APIs
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

#ifndef _XCL_HAL2_MPSOC_H_
#define _XCL_HAL2_MPSOC_H_

#include "xclhal2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	XRT_MAX_NAME_LENGTH	32
#define	XRT_MAX_PATH_LENGTH	255

#define	SOFT_KERNEL_FILE_PATH	"/lib/firmware/xilinx/softkernel/"
#define	SOFT_KERNEL_FILE_NAME	"sk"

#define	SOFT_KERNEL_REG_SIZE	4096

struct xclSKCmd {
    uint32_t	opcode;
    uint32_t	start_cuidx;
    uint32_t	cu_nums;
    uint64_t	xclbin_paddr;
    size_t	xclbin_size;
    char	krnl_name[XRT_MAX_NAME_LENGTH];
};

enum xrt_scu_state {
    XRT_SCU_STATE_DONE,
};

/**
 * xclGetHostBO() - Get Host allocated BO
 *
 * @handle:        Device handle
 * @paddr:         Physical address
 * @size:          Size of the Buffer
 * Return:         Buffer Object handler
 *
 * Get host allocated buffer by physical address.
 * NOTE: This is WIP and do not directly used it.
 */
XCL_DRIVER_DLLESPEC unsigned int xclGetHostBO(xclDeviceHandle handle, uint64_t paddr, size_t size);

/**
 * xclSKGetCmd() - Get a command for soft kernel
 *
 * @handle:        Device handle
 * @cmd:           Pointer to the command
 * Return:         0 on success or appropriate error number
 */
XCL_DRIVER_DLLESPEC int xclSKGetCmd(xclDeviceHandle handle, xclSKCmd *cmd);

/**
 * xclSKCreate() - Create a soft kernel compute unit
 *
 * @handle:        Device handle
 * @boHandle:      Bo handle for the CU's reg file
 * @cu_idx:        CU index
 * Return:         0 on success or appropriate error number
 */
XCL_DRIVER_DLLESPEC int xclSKCreate(xclDeviceHandle handle, unsigned int boHandle, uint32_t cu_idx);

/**
 * xclSKReport() - Report a soft kernel compute unit state change
 *
 * @handle:        Device handle
 * @cu_idx:        CU index
 * @state:         CU state
 * Return:         0 on success or appropriate error number
 */
XCL_DRIVER_DLLESPEC int xclSKReport(xclDeviceHandle handle, uint32_t cu_idx, xrt_scu_state state);

#ifdef __cplusplus
}
#endif

#endif
