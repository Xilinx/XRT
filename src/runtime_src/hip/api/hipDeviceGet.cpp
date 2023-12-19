// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.
#include "hip/hip.h"
#include "hip/core/device.h"

#include "core/common/error.h"

namespace xrthip_core {

// Returns a handle to compute device
// Throws on error
static void
hipDeviceGet(hipDevice_t* device, int ordinal)
{
  throw std::runtime_error("Not implemented");
}
  
} // xrthip_core


hipError_t
hipDeviceGet(hipDevice_t* device, int ordinal)
{
  try {
    xrthip_core::hipDeviceGet(device, ordinal);
    return hipSuccess;
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorInvalidDevice;
}
