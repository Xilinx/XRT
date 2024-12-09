// Copyright (C) 2021 Xilinx, Inc
// SPDX-License-Identifier: Apache-2.0

// This file implements XRT error APIs as declared in
// core/include/experimental/xrt_system.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_ini.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/xrt/experimental/xrt_system.h"

#include "core/common/system.h"

namespace xrt::system {

unsigned int
enumerate_devices()
{
  return static_cast<unsigned int>(xrt_core::get_total_devices(true/*is_user*/).second);
}

} // xrt::system

////////////////////////////////////////////////////////////////
// xrt_message C API implmentations (xrt_message.h)
////////////////////////////////////////////////////////////////
