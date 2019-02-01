/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include <CL/opencl.h>
#include "xocl/core/pipe.h"
#include "xocl/core/device.h"
#include "xocl/core/error.h"

#include "xocl/api/detail/device.h"

namespace xocl {

static void
validOrError(cl_device_id device,
             cl_mem_flags flags,
             cl_uint packet_size,
             cl_uint max_packets,
             const cl_pipe_attributes *attributes)
{
  if (!config::api_checks())
    return;

  xocl::detail::device::validOrError(device);

  cl_mem_flags f = CL_MEM_RTE_MBUF_READ_ONLY | CL_MEM_RTE_MBUF_WRITE_ONLY;
  if (flags & (~f))
    throw error(CL_INVALID_VALUE);

  if (flags & (flags - 1))
    throw error(CL_INVALID_VALUE);

  if (!attributes)
    throw error(CL_INVALID_VALUE);

  if (*attributes != static_cast<decltype(*attributes)>(CL_PIPE_ATTRIBUTE_DPDK_ID))
    throw error(CL_INVALID_VALUE);

}

static cl_pipe
clCreateHostPipe(cl_device_id device,
                 cl_mem_flags flags,
                 cl_uint packet_size,
                 cl_uint max_packets,
                 const cl_pipe_attributes *attributes) CL_API_SUFFIX__VERSION_1_0
{
  validOrError(device,flags,packet_size,max_packets,attributes);

  attributes++; // ???

  auto pipe = std::make_unique<pmd::pipe>(nullptr,xocl::xocl(device),flags,max_packets,*attributes);
  return pipe.release();
}

} // xocl

cl_pipe
clCreateHostPipe(cl_device_id device,
                 cl_mem_flags flags,
                 cl_uint packet_size,
                 cl_uint max_packets,
                 const cl_pipe_attributes *attributes, //TODO: properties?
                 cl_int *errcode_ret)
{
  try {
    auto p = xocl::clCreateHostPipe(device, flags, packet_size, max_packets, attributes);
    xocl::assign(errcode_ret,CL_SUCCESS);
    return p;
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
  }
  return nullptr;
}
