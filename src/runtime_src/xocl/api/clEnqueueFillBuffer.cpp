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
#include "xocl/core/event.h"
#include "xocl/core/memory.h"
#include "xocl/core/device.h"
#include "xocl/core/command_queue.h"
#include "detail/command_queue.h"
#include "detail/memory.h"
#include "detail/event.h"

#include "api.h"
#include "enqueue.h"
#include "plugin/xdp/appdebug.h"
#include "plugin/xdp/profile.h"

#include <CL/opencl.h>

namespace xocl {

static cl_uint
getDeviceMemBaseAddrAlign(cl_device_id device)
{
  cl_uint size = 0;
  api::clGetDeviceInfo(device,CL_DEVICE_MEM_BASE_ADDR_ALIGN,sizeof(cl_uint),&size,nullptr);
  return size;
}

static void
validOrError(cl_command_queue command_queue,
             cl_mem           buffer,
             const void*      pattern,
             size_t           pattern_size,
             size_t           offset,
             size_t           size,
             cl_uint          num_events_in_wait_list,
             const cl_event*  event_wait_list,
             cl_event*        event)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host
  // command-queue.
  //
  // CL_INVALID_CONTEXT if the context associated with command_queue
  // and buffer are not the same or if the context associated with
  // command_queue and events in event_wait_list are not the same.
  //
  // CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and
  // num_events_in_wait_list > 0, or event_wait_list is not NULL and
  // num_events_in_wait_list is 0, or if event objects in
  // event_wait_list are not valid events.
  detail::event::validOrError(command_queue,num_events_in_wait_list,event_wait_list);

  // CL_INVALID_MEM_OBJECT if buffer is not a valid buffer object.
  detail::memory::validOrError(buffer);

  // CL_INVALID_VALUE if offset or offset + size require accessing
  // elements outside the buffer buffer object respectively.
  if (offset+size > xocl(buffer)->get_size())
    throw error(CL_INVALID_VALUE,"invalid offset and size");

  // CL_INVALID_VALUE if pattern is NULL or if pattern_size is 0 or if
  // pattern_size is not one of {1, 2, 4, 8, 16, 32, 64, 128}.
  if (!pattern || !pattern_size)
    throw error(CL_INVALID_VALUE,"invalid pattern or pattern_size");
  auto sizes = {1,2,4,8,16,32,64,128};
  if (std::find(sizes.begin(),sizes.end(),pattern_size)==sizes.end())
    throw error(CL_INVALID_VALUE,"invalid pattern or pattern_size");

  // CL_INVALID_VALUE if offset and size are not a multiple of
  // pattern_size.
  if (offset % pattern_size)
    throw error(CL_INVALID_VALUE,"invalid offset");
  if (size % pattern_size)
    throw error(CL_INVALID_VALUE,"invalid size");

  // CL_MISALIGNED_SUB_BUFFER_OFFSET if buffer is a sub-buffer object
  // and offset specified when the sub-buffer object is created is not
  // aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device
  // associated with queue.
  auto align = getDeviceMemBaseAddrAlign(xocl(command_queue)->get_device());
  if (xocl(buffer)->is_sub_buffer() && (xocl(buffer)->get_sub_buffer_offset() % align))
    throw error(CL_MISALIGNED_SUB_BUFFER_OFFSET,"bad sub buffer offset");

  // CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to
  // allocate memory for data store associated with buffer.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clEnqueueFillBuffer(cl_command_queue command_queue,
                    cl_mem           buffer,
                    const void*      pattern,
                    size_t           pattern_size,
                    size_t           offset,
                    size_t           size,
                    cl_uint          num_events_in_wait_list,
                    const cl_event*  event_wait_list,
                    cl_event*        event)
{
  validOrError(command_queue,buffer,pattern,pattern_size,offset,size
               ,num_events_in_wait_list,event_wait_list,event);

  auto uevent = xocl::create_hard_event
    (command_queue,CL_COMMAND_FILL_BUFFER,num_events_in_wait_list,event_wait_list);
  xocl::enqueue::set_event_action
    (uevent.get(),xocl::enqueue::action_fill_buffer,buffer,pattern,pattern_size,offset,size);
  xocl::appdebug::set_event_action
    (uevent.get(),xocl::appdebug::action_fill_buffer,buffer,pattern,pattern_size,offset,size);

  uevent->queue();
  xocl::assign(event,uevent.get());
  return CL_SUCCESS;
}

} // api_impl

cl_int
clEnqueueFillBuffer(cl_command_queue command_queue,
                    cl_mem           buffer,
                    const void*      pattern,
                    size_t           pattern_size,
                    size_t           offset,
                    size_t           size,
                    cl_uint          num_events_in_wait_list,
                    const cl_event*  event_wait_list,
                    cl_event*        event)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueFillBuffer
      (command_queue,buffer,pattern,pattern_size,offset,size
       ,num_events_in_wait_list,event_wait_list,event);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
