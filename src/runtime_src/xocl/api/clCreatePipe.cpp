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
#include "xocl/config.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "xocl/core/memory.h"

#include "detail/context.h"
#include "detail/memory.h"

#include "xrt/util/memory.h"
#include "api.h"
#include "profile.h"

// Must move out of impl
#include "impl/cpu_pipes.h"

#include <cstdlib>

namespace xocl {

static cl_uint
getDevicePipeMaxPacketSize(cl_device_id device) 
{
  cl_uint size = 0;
  api::clGetDeviceInfo(device,CL_DEVICE_PIPE_MAX_PACKET_SIZE,sizeof(cl_uint),&size,nullptr);
  return size;
}

static void
validOrError(cl_context                context,
             cl_mem_flags              flags,
             cl_uint                   pipe_packet_size,
             cl_uint                   pipe_max_packets,
             const cl_pipe_attributes *attributes,
             cl_int *                 errcode_ret)
{
  if( !xocl::config::api_checks())
    return;

  // CL_INVALID_CONTEXT if context is not a valid context.
  detail::context::validOrError(context);

  // CL_INVALID_VALUE if values specified in flags are not as defined
  // above
  detail::memory::validOrError(flags);

  // CL_INVALID_VALUE if properties is not NULL.
  if (attributes)
    throw error(CL_INVALID_VALUE,"properties must be nullptr");

  // CL_INVALID_PIPE_SIZE if pipe_packet_size is 0 or the
  // pipe_packet_size exceeds CL_DEVICE_PIPE_MAX_PACKET_SIZE value
  // specified in table 4.3 (see clGetDeviceInfo) for all devices in
  // context or if pipe_max_packets is 0.
  if (!pipe_packet_size)
    //throw error(CL_INVALID_PIPE_SIZE,"pipe_packet_size must be > 0");
    throw error(CL_INVALID_VALUE,"pipe_packet_size must be > 0");
  auto dr = xocl(context)->get_device_range();
  if (std::any_of(dr.begin(),dr.end(),
       [pipe_packet_size](device* d)
       {return pipe_packet_size > getDevicePipeMaxPacketSize(d); }))
    //throw error(CL_INVALID_PIPE_SIZE,"pipe_packet_size must be <= max packet size for all devices");
    throw error(CL_INVALID_VALUE,"pipe_packet_size must be <= max packet size for all devices");

  // CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to
  // allocate memory for the pipe object.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_mem
clCreatePipe(cl_context                context,
             cl_mem_flags              flags,
             cl_uint                   pipe_packet_size,
             cl_uint                   pipe_max_packets,
             const cl_pipe_attributes *attributes,
             cl_int *                  errcode_ret)
{
  validOrError(context,flags,pipe_packet_size,pipe_max_packets,attributes,errcode_ret);

  auto upipe = xrt::make_unique<xocl::pipe>(xocl::xocl(context),flags,pipe_packet_size,pipe_max_packets);
  cl_mem pipe = upipe.get();

  // TODO: here we allocate a pipe even if it isn't a memory mapped pipe,
  // it would be nice to not allocate the pipe if it's a hardware pipe.
  size_t nbytes = upipe->get_pipe_packet_size() * (upipe->get_pipe_max_packets()+8);
  void* user_ptr=nullptr;
  int status = posix_memalign(&user_ptr, 128, (sizeof(cpu_pipe_t)+nbytes));
  if (status)
    throw xocl::error(CL_MEM_OBJECT_ALLOCATION_FAILURE);
  upipe->set_pipe_host_ptr(user_ptr);

  xocl::assign(errcode_ret,CL_SUCCESS);
  return upipe.release();
}

} // xocl

cl_mem
clCreatePipe(cl_context                context,
             cl_mem_flags              flags,
             cl_uint                   pipe_packet_size,
             cl_uint                   pipe_max_packets,
             const cl_pipe_attributes *attributes,
             cl_int *                  errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clCreatePipe
      (context,flags,pipe_packet_size,pipe_max_packets,attributes,errcode_ret);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
  }
  return nullptr;
}


