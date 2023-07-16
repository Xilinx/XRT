// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_PSCONTEXT_H_
#define XRT_PSCONTEXT_H_

#include "xrt/detail/config.h"
#include "xrt/detail/pimpl.h"
#include "xclhal2.h"

#ifdef __cplusplus
#include <memory>

/*
 * PS Context Data Structure to be derived from for user's xrtHandle class
 * User will need to declare an xrtHandles class derived from xrt::pscontext class
 * Example:
 * class xrtHandles : public xrt::pscontext
 * {
 * public:
 *   xrt::device dhdl;
 *   xrt::kernel kernel;
 *   xrtHandles(xclDeviceHandle dhdl_in, const xuid_t xclbin_uuid)
 *     : dhdl(dhdl_in)
 *     , kernel(dhdl,xclbin_uuid,"kernel name")
 *   {
 *   }
 * };
 *
 * This xrtHandles is the return type for kernel_ini function.
 * xrt::pscontext *kernel_init(xclDeviceHandle dhdl, const xuid_t xclbin_uuid) {
 *   xrtHandles *handles = new xrtHandles(dhdl, xclbin_uuid);
 *
 *   return(handles);
 * }
 *
 * 
 */

namespace xrt {

class pscontext_impl;
class pscontext : public detail::pimpl<pscontext_impl> {
public:
  pscontext() = default;
  virtual ~pscontext() {}
};

}
 
#else
# error xrt_pscontext is only implemented for C++
#endif // __cplusplus

#endif
