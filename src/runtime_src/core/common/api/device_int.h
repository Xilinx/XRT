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

#ifndef _XRT_COMMON_DEVICE_INT_H_
#define _XRT_COMMON_DEVICE_INT_H_

// This file defines implementation extensions to the XRT Device APIs.
#include "core/include/experimental/xrt_device.h"

namespace xrt_core { namespace device_int {

// Retrieve xrt_core::device from xrtDeviceHandle
std::shared_ptr<xrt_core::device>
get_core_device(xrtDeviceHandle dhdl);

// Get underlying shim device handle 
xclDeviceHandle
get_xcl_device_handle(xrtDeviceHandle dhdl);

}} // device_int, xrt_core

#endif
