/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

// Copyright 2017-2020 Xilinx, Inc. All rights reserved.

#include "xocl/config.h"
#include "xocl/core/memory.h"
#include "xocl/core/device.h"
#include "xocl/api/detail/memory.h"
#include "xocl/api/detail/device.h"

#include "xrt/device/device.h"

namespace xocl {

void
validOrError(cl_mem mem,
             cl_device_id device,
             size_t size,
             void* address)
{
  if (!config::api_checks())
    return;

  detail::memory::validOrError(mem);
  detail::device::validOrError(device);

  if (!address)
    throw error(CL_INVALID_VALUE,"address argument in nullptr");
  if (size != sizeof(uintptr_t))
    throw error(CL_INVALID_VALUE,"size of address argument must be sizeof(uintptr_t)");

  if (!xocl(mem)->get_buffer_object_or_null(xocl(device)))
    throw error(CL_INVALID_MEM_OBJECT,"mem object is not associated with device");
}

cl_int
clGetMemObjDeviceAddress(cl_mem mem,
                         cl_device_id device,
                         size_t size,
                         void* address)
{
  validOrError(mem,device,size,address);

  if (auto boh = xocl(mem)->get_buffer_object_or_null(xocl(device))) {
    auto xdevice = xocl(device)->get_xdevice();
    auto addr = reinterpret_cast<uintptr_t*>(address);
    *addr = xdevice->getDeviceAddr(boh);
    return CL_SUCCESS;
  }

  throw error(CL_INVALID_MEM_OBJECT,"mem object is not associated with device");
}

} // xocl

namespace xlnx {

cl_int
clGetMemObjDeviceAddress(cl_mem mem,
                         cl_device_id device,
                         size_t size,
                         void* address)
{
  try {
    return xocl::clGetMemObjDeviceAddress(mem,device,size,address);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}

} // xlnx


cl_int
xclGetMemObjDeviceAddress(cl_mem mem,
                          cl_device_id device,
                          size_t size,
                          void* address)
{
  return xlnx::clGetMemObjDeviceAddress(mem,device,size,address);
}


