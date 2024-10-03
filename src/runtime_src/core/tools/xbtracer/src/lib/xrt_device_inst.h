// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "xrt/xrt_device.h"

/*
 * device class method aliases.
 * */
using xrt_device_ctor = xrt::device* (*) (void *, unsigned int);
using xrt_device_ctor_bdf = xrt::device* (*) (void *, const std::string&);
using xrt_device_ctor_dhdl = xrt::device* (*) (void *, xclDeviceHandle);
using xrt_device_register_xclbin = xrt::uuid (xrt::device::*)\
        (const xrt::xclbin&);
using xrt_device_load_xclbin_axlf = xrt::uuid (xrt::device::*) (const axlf*);
using xrt_device_load_xclbin_fnm = xrt::uuid (xrt::device::*)\
        (const std::string &);
using xrt_device_load_xclbin_obj = xrt::uuid (xrt::device::*)\
        (const xrt::xclbin&);
using xrt_device_get_xclbin_uuid = xrt::uuid (xrt::device::*) (void) const;
using xrt_device_reset = xrt::uuid (xrt::device::*) (void);
using xrt_device_dtor = xrt::uuid (xrt::device::*) (void);

/*
 * struct xrt_device_dtor definition.
 */
struct xrt_device_ftbl {
  xrt_device_ctor             ctor;
  xrt_device_ctor_bdf         ctor_bdf;
  xrt_device_ctor_dhdl        ctor_dhdl;
  xrt_device_register_xclbin  register_xclbin;
  xrt_device_load_xclbin_axlf load_xclbin_axlf;
  xrt_device_load_xclbin_fnm  load_xclbin_fnm;
  xrt_device_load_xclbin_obj  load_xclbin_obj;
  xrt_device_get_xclbin_uuid  get_xclbin_uuid;
  xrt_device_reset            reset;
  xrt_device_dtor             dtor;
};
