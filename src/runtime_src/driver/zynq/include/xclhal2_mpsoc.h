/*
 * Copyright (C) 2015-2018, Xilinx Inc - All rights reserved
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

#ifdef __cplusplus
}
#endif

#endif
