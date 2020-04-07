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

#include "xocl/config.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "xocl/core/memory.h"
#include "core/common/memalign.h"

#include "detail/context.h"
#include "detail/memory.h"

#include "api.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"
#include <CL/opencl.h>
#include <cstdlib>
#include <mutex>
#include <deque>

#ifdef _WIN32
# pragma warning ( disable : 4200 4505 )
#endif

namespace {

struct cpu_pipe_reserve_id_t {
  std::size_t head;
  std::size_t tail;
  std::size_t next;
  unsigned int size;
  unsigned int ref;
};

struct cpu_pipe_t {
  std::mutex rd_mutex;
  std::mutex wr_mutex;
  std::size_t pkt_size;
  std::size_t pipe_size;
  std::size_t head;
  std::size_t tail;

  std::deque<cpu_pipe_reserve_id_t*> rd_rids;
  std::deque<cpu_pipe_reserve_id_t*> wr_rids;

  char buf[0];
};

}

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
             const cl_pipe_properties *properties,
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
  if (properties)
    throw error(CL_INVALID_VALUE,"properties must be nullptr");

  // CL_INVALID_PIPE_SIZE if pipe_packet_size is 0 or the
  // pipe_packet_size exceeds CL_DEVICE_PIPE_MAX_PACKET_SIZE value
  // specified in table 4.3 (see clGetDeviceInfo) for all devices in
  // context or if pipe_max_packets is 0.
  if (!pipe_packet_size)
    //throw error(CL_INVALID_PIPE_SIZE,"pipe_packet_size must be > 0");
    throw error(CL_INVALID_VALUE,"pipe_packet_size must be > 0");
  for (auto d : xocl(context)->get_device_range())
    if (pipe_packet_size > getDevicePipeMaxPacketSize(d))
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
             const cl_pipe_properties *properties,
             cl_int *                  errcode_ret)
{
  validOrError(context,flags,pipe_packet_size,pipe_max_packets,properties,errcode_ret);

  auto upipe = std::make_unique<xocl::pipe>(xocl::xocl(context),flags,pipe_packet_size,pipe_max_packets);

  // TODO: here we allocate a pipe even if it isn't a memory mapped pipe,
  // it would be nice to not allocate the pipe if it's a hardware pipe.
  size_t nbytes = upipe->get_pipe_packet_size() * (upipe->get_pipe_max_packets()+8);
  void* user_ptr=nullptr;
  int status = xrt_core::posix_memalign(&user_ptr, 128, (sizeof(cpu_pipe_t)+nbytes));
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
             const cl_pipe_properties *properties,
             cl_int *                  errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clCreatePipe
      (context,flags,pipe_packet_size,pipe_max_packets,properties,errcode_ret);
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
