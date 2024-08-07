// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "xrt/xrt_device.h"
/*
 * device class method aliases.
 * */
using xrt_device_ctor = xrt::device* (*) (void *, unsigned int);
using xrt_device_load_xclbin_fnm = xrt::uuid (xrt::device::*)\
        (const std::string &);
using xrt_device_dtor = xrt::uuid (xrt::device::*) (void);

/*
 * struct xrt_device_dtor definition.
 */
struct xrt_device_ftbl {
  xrt_device_ctor             ctor;
  xrt_device_load_xclbin_fnm  load_xclbin_fnm;
  xrt_device_dtor             dtor;
};
