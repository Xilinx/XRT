// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
#ifndef _XRT_COMMON_DEVICE_INT_H_
#define _XRT_COMMON_DEVICE_INT_H_

// This file defines implementation extensions to the XRT Device APIs.
#include "core/include/xrt/xrt_device.h"
#include <chrono>
#include <condition_variable>

namespace xrt_core { namespace device_int {

// Retrieve xrt_core::device from xrtDeviceHandle
std::shared_ptr<xrt_core::device>
get_core_device(xrtDeviceHandle dhdl);

// Get underlying shim device handle
xclDeviceHandle
get_xcl_device_handle(xrtDeviceHandle dhdl);

// Call exec_wait() safely from multiple threads.  This function
// returns when exec_wait encounters any command completion or
// otherwise times out.
std::cv_status
exec_wait(const xrt::device& device, const std::chrono::milliseconds& timeout_ms);

}} // device_int, xrt_core

#endif
