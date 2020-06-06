/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#ifndef _XRT_XCLBIN_H_
#define _XRT_XCLBIN_H_

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * xrtGetXclbinUUID() - Get UUID of xclbin image running on device
 *
 * @handle: Device handle
 * @out:    Return xclbin id in this uuid_t struct
 * Return:  0 on success or appropriate error number
 */
XCL_DRIVER_DLLESPEC
int
xrtXclbinUUID(xclDeviceHandle handle, xuid_t out);

#ifdef __cplusplus
}
#endif

#endif
