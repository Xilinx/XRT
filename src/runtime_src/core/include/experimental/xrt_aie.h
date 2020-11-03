/**
 * Copyright (C) 2020 Xilinx, Inc
 * Author(s): Larry Liu
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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

#ifndef _XRT_AIE_H_
#define _XRT_AIE_H_

#include "xrt.h"
#include "experimental/xrt_uuid.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"
#include "experimental/xrt_graph.h"

/**
 * xrtAIESyncBO() - Transfer data between DDR and Shim DMA channel
 *
 * @handle:          Handle to the device
 * @bohdl:           BO handle.
 * @gmioName:        GMIO name
 * @dir:             GM to AIE or AIE to GM
 * @size:            Size of data to synchronize
 * @offset:          Offset within the BO
 *
 * Return:          0 on success, or appropriate error number.
 *
 * Synchronize the buffer contents between GMIO and AIE.
 * Note: Upon return, the synchronization is done or error out
 */
int
xrtAIESyncBO(xrtDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset);

/**
 * xrtResetAIEArray() - Reset the AIE array
 *
 * @handle:         Handle to the device.
 *
 * Return:          0 on success, or appropriate error number.
 */
int
xrtAIEResetArray(xrtDeviceHandle handle);

/* Provide this API for backward compatibility. */
int
xrtSyncBOAIE(xrtDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset);

/* Provide this API for backward compatibility. */
int
xrtResetAIEArray(xrtDeviceHandle handle);

#endif
