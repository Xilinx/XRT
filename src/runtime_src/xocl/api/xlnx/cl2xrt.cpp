/**
 * Copyright (C) 2021 Xilinx, Inc
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
#define XOCL_SOURCE  // exported for end application use
#include "xocl/config.h"
#include "xocl/core/device.h"
#include "xocl/core/kernel.h"
#include "xocl/core/memory.h"

#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_device.h"
#include "core/include/xrt/xrt_kernel.h"

#include <CL/cl2xrt.hpp>

namespace xrt { namespace opencl {

xrt::device
get_xrt_device(cl_device_id device)
{
  if (auto xdevice = xocl::xocl(device)->get_xrt_device())
    return xdevice;

  throw xrt_core::error(ENODEV, "OpenCL context has not been created, xrt::device does not exist");
}

xrt::bo
get_xrt_bo(cl_device_id device, cl_mem mem)
{
  if (auto bo = xocl::xocl(mem)->get_buffer_object_or_null(xocl::xocl(device)))
    return bo;

  throw xrt_core::error("OpenCL memory object is not associated with a buffer object");
}

xrt::kernel
get_xrt_kernel(cl_device_id device, cl_kernel kernel)
{
  return xocl::xocl(kernel)->get_xrt_kernel(xocl::xocl(device));
}

xrt::run
get_xrt_run(cl_device_id device, cl_kernel kernel)
{
  return xrt_core::kernel_int::clone(xocl::xocl(kernel)->get_xrt_run(xocl::xocl(device)));
}

}} // opencl, xrt
