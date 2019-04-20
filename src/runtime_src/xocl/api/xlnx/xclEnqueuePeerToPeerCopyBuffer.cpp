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
#include "xocl/core/event.h"
#include "xocl/core/memory.h"
#include "xocl/core/device.h"
#include "xocl/core/context.h"
#include "xocl/core/command_queue.h"
#include "detail/command_queue.h"
#include "detail/memory.h"
#include "detail/context.h"
#include "detail/event.h"

#include "api.h"
#include "enqueue.h"
#include "xocl/api/plugin/xdp/profile.h"
#include "xocl/api/plugin/xdp/appdebug.h"

namespace xocl {

static cl_uint
getDeviceMemBaseAddrAlign(cl_device_id device)
{
  cl_uint size = 0;
  api::clGetDeviceInfo(device,CL_DEVICE_MEM_BASE_ADDR_ALIGN,sizeof(cl_uint),&size,nullptr);
  return size;
}

static void
validOrError(cl_command_queue    command_queue,
             cl_mem              src_buffer,
             cl_mem              dst_buffer,
             size_t              src_offset,
             size_t              dst_offset,
             size_t              size,
             cl_uint             num_events_in_wait_list,
             const cl_event *    event_wait_list,
             cl_event *          event_parameter)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host command-queue.
  detail::command_queue::validOrError(command_queue);

  // CL_INVALID_MEM_OBJECT if src_buffer and dst_buffer are not valid
  // buffer objects.
  detail::memory::validOrError({src_buffer,dst_buffer});

  // CL_INVALID_CONTEXT if the context associated with command_queue,
  // src_buffer, and dst_buffer are not the same or if the context
  // associated with command_queue and events in event_wait_list are
  // not the same.
  detail::context::validOrError(xocl(command_queue)->get_context(),{src_buffer, dst_buffer});

  // CL_INVALID_VALUE if src_offset, dst_offset, size, src_offset +
  // size, or dst_offset + size require accessing elements outside the
  // src_buffer and dst_buffer buffer objects respectively.
  if((src_offset+size)>(xocl(src_buffer)->get_size()))
    throw error(CL_INVALID_VALUE,"xclEnqueuePeerToPeerCopyBuffer src_offset invalid src_size");
  if((dst_offset+size)>(xocl(dst_buffer)->get_size()))
    throw error(CL_INVALID_VALUE,"xclEnqueuePeerToPeerCopyBuffer dest_offset invalid dest_size");

  // CL_INVALID_VALUE if size is 0.
  if (!size)
    throw error(CL_INVALID_VALUE,"size==0");

  // CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and
  // num_events_in_wait_list is > 0, or event_wait_list is not NULL
  // and num_events_in_wait_list is 0, or if event objects in
  // event_wait_list are not valid events.
  detail::event::validOrError(num_events_in_wait_list,event_wait_list);

  // CL_MISALIGNED_SUB_BUFFER_OFFSET if src_buffer is a sub-buffer
  // object and offset specified when the sub-buffer object is created
  // is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device
  // associated with queue.
  //
  // CL_MISALIGNED_SUB_BUFFER_OFFSET if dst_buffer is a sub-buffer
  // object and offset specified when the sub-buffer object is created
  // is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device
  // associated with queue.
  auto align = getDeviceMemBaseAddrAlign(xocl(command_queue)->get_device());
  if (xocl(src_buffer)->is_sub_buffer() && (xocl(src_buffer)->get_sub_buffer_offset() % align))
    throw error(CL_MISALIGNED_SUB_BUFFER_OFFSET,"xclEnqueuePeerToPeerCopyBuffer bad src sub buffer offset");
  if (xocl(dst_buffer)->is_sub_buffer() && (xocl(dst_buffer)->get_sub_buffer_offset() % align))
    throw error(CL_MISALIGNED_SUB_BUFFER_OFFSET,"xclEnqueuePeerToPeerCopyBuffer bad dst sub buffer offset");

  // CL_MEM_COPY_OVERLAP if src_buffer and dst_buffer are the same
  // buffer or subbuffer object and the source and destination regions
  // overlap or if src_buffer and dst_buffer are different sub-buffers
  // of the same associated buffer object and they overlap. The
  // regions overlap if src_offset <= dst_offset <= src_offset + size -
  // 1, or if dst_offset <= src_offset <= dst_offset + size - 1.
  if ((src_buffer==dst_buffer) &&
      (( (src_offset<=dst_offset) && (dst_offset<=src_offset+size-1) ) ||
       ( (dst_offset<=src_offset) && (src_offset<=dst_offset+size-1) )
      ))
    throw xocl::error(CL_MEM_COPY_OVERLAP,"xclEnqueuePeerToPeerCopyBuffer mem copy overlap");

  // CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to
  // allocate memory for data store associated with src_buffer or
  // dst_buffer.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
xclEnqueuePeerToPeerCopyBuffer(cl_command_queue    command_queue,
                    cl_mem              src_buffer,
                    cl_mem              dst_buffer,
                    size_t              src_offset,
                    size_t              dst_offset,
                    size_t              size,
                    cl_uint             num_events_in_wait_list,
                    const cl_event *    event_wait_list,
                    cl_event *          event_parameter)
{
  validOrError
    (command_queue,src_buffer,dst_buffer,src_offset,dst_offset,size,num_events_in_wait_list,event_wait_list,event_parameter);

  auto uevent = xocl::create_hard_event
    (command_queue,CL_COMMAND_COPY_BUFFER,num_events_in_wait_list,event_wait_list);
  xocl::enqueue::set_event_action
    (uevent.get(),xocl::enqueue::action_copy_p2p_buffer,src_buffer,dst_buffer,src_offset,dst_offset,size);
  xocl::profile::set_event_action
    (uevent.get(),xocl::profile::action_copy,src_buffer,dst_buffer,src_offset,dst_offset,size,false);
  xocl::appdebug::set_event_action
    (uevent.get(),xocl::appdebug::action_copybuf,src_buffer,dst_buffer,src_offset,dst_offset,size);

  uevent->queue();
  xocl::assign(event_parameter,uevent.get());
  return CL_SUCCESS;
}

} // xocl

cl_int
xclEnqueuePeerToPeerCopyBuffer(cl_command_queue    command_queue,
                    cl_mem              src_buffer,
                    cl_mem              dst_buffer,
                    size_t              src_offset,
                    size_t              dst_offset,
                    size_t              size,
                    cl_uint             num_events_in_wait_list,
                    const cl_event *    event_wait_list,
                    cl_event *          event_parameter)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::xclEnqueuePeerToPeerCopyBuffer
      (command_queue,src_buffer,dst_buffer,src_offset,dst_offset,size,
       num_events_in_wait_list,event_wait_list,event_parameter);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
