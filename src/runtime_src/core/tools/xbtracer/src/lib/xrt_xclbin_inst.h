// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "xrt/experimental/xrt_xclbin.h"

/*
 * xclbin class method aliases.
 */
using xrt_xclbin_ctor_fnm = xrt::xclbin* (*)(void*, const std::string&);
using xrt_xclbin_ctor_raw = xrt::xclbin* (*)(void*, const std::vector<char>&);
using xrt_xclbin_ctor_axlf = xrt::xclbin* (*)(void*, const axlf*);
using xrt_xclbin_get_kernels = std::vector<xrt::xclbin::kernel>\
  (xrt::xclbin::*)(void) const;
using xrt_xclbin_get_kernel = xrt::xclbin::kernel (xrt::xclbin::*)\
  (const std::string&) const;
using xrt_xclbin_get_ips = std::vector<xrt::xclbin::ip> (xrt::xclbin::*)\
  (void) const;
using xrt_xclbin_get_ip = xrt::xclbin::ip (xrt::xclbin::*)\
  (const std::string&) const;
using xrt_xclbin_get_mems = std::vector<xrt::xclbin::mem> (xrt::xclbin::*)\
  (void) const;
using xrt_xclbin_get_xsa_name = std::string (xrt::xclbin::*)(void) const;
using xrt_xclbin_get_fpga_device_name = std::string (xrt::xclbin::*)\
  (void) const;
using xrt_xclbin_get_uuid = xrt::uuid (xrt::xclbin::*)(void) const;
using xrt_xclbin_get_interface_uuid = xrt::uuid (xrt::xclbin::*)(void) const;
using xrt_xclbin_get_target_type = xrt::xclbin::target_type (xrt::xclbin::*)\
  (void) const;
using xrt_xclbin_get_axlf = const axlf* (xrt::xclbin::*)(void) const;

/*
 * struct xrt_xclbin_ftbl definition.
 */
struct xrt_xclbin_ftbl
{
  xrt_xclbin_ctor_fnm ctor_fnm;
  xrt_xclbin_ctor_raw ctor_raw;
  xrt_xclbin_ctor_axlf ctor_axlf;
  xrt_xclbin_get_kernels get_kernels;
  xrt_xclbin_get_kernel get_kernel;
  xrt_xclbin_get_ips get_ips;
  xrt_xclbin_get_ip get_ip;
  xrt_xclbin_get_mems get_mems;
  xrt_xclbin_get_xsa_name get_xsa_name;
  xrt_xclbin_get_fpga_device_name get_fpga_device_name;
  xrt_xclbin_get_uuid get_uuid;
  xrt_xclbin_get_target_type get_target_type;
  xrt_xclbin_get_axlf get_axlf;
};
