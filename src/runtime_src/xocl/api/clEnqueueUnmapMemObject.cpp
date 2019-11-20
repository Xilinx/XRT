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
#include "xocl/core/command_queue.h"
#include "xocl/core/device.h"

#include "detail/event.h"
#include "detail/memory.h"

#include "enqueue.h"
#include "plugin/xdp/appdebug.h"
#include "plugin/xdp/profile.h"

#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_command_queue  command_queue,
             cl_mem            memobj,
             void *            mapped_ptr,
             cl_uint           num_events_in_wait_list,
             const cl_event *  event_wait_list,
             cl_event *        event_parameter)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host
  // command-queue.
  //
  // CL_INVALID_CONTEXT if the context associated with command_queue
  // and memobj are not the same or if the context associated with
  // command_queue and events in event_wait_list are not the same.
  //
  // CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and
  // num_events_in_wait_list > 0, or event_wait_list is not NULL and
  // num_events_in_wait_list is 0, or if event objects in
  // event_wait_list are not valid events.
  detail::event::validOrError(command_queue,num_events_in_wait_list,event_wait_list);

  // CL_INVALID_MEM_OBJECT if memobj is not a valid memory object or
  // is a pipe object.
  detail::memory::validOrError(memobj);

  // CL_INVALID_VALUE if mapped_ptr is not a valid pointer returned by
  // clEnqueueMapBuffer or clEnqueueMapImage for memobj.
  if (!xocl(command_queue)->get_device()->is_mapped(mapped_ptr))
    throw xocl::error(CL_INVALID_VALUE,"mapped_ptr is not a valid ptr");

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.

}

static cl_int
clEnqueueUnmapMemObject(cl_command_queue  command_queue,
                        cl_mem            memobj,
                        void *            mapped_ptr,
                        cl_uint           num_events_in_wait_list,
                        const cl_event *  event_wait_list,
                        cl_event *        event_parameter)
{
  validOrError(command_queue,memobj,mapped_ptr,
               num_events_in_wait_list,event_wait_list,event_parameter);

  auto uevent = xocl::create_hard_event
    (command_queue,CL_COMMAND_UNMAP_MEM_OBJECT,num_events_in_wait_list,event_wait_list);
  xocl::enqueue::set_event_action
    (uevent.get(),xocl::enqueue::action_unmap_buffer,memobj,mapped_ptr);
  xocl::profile::set_event_action
    (uevent.get(),xocl::profile::action_unmap,memobj);
  xocl::appdebug::set_event_action
    (uevent.get(),xocl::appdebug::action_unmap,memobj);

  uevent->queue();
  xocl::assign(event_parameter,uevent.get());
  return CL_SUCCESS;
}

} // xocl

cl_int
clEnqueueUnmapMemObject(cl_command_queue  command_queue,
                        cl_mem            memobj,
                        void *            mapped_ptr,
                        cl_uint           num_events_in_wait_list,
                        const cl_event *  event_wait_list,
                        cl_event *        event_parameter)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueUnmapMemObject
      (command_queue,memobj,mapped_ptr,num_events_in_wait_list,event_wait_list,event_parameter);
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
