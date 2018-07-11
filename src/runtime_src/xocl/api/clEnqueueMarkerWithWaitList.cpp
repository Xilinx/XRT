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
#include "xocl/core/command_queue.h"

#include "detail/event.h"

#include "xoclProfile.h"


namespace xocl {

static void
validOrError(cl_command_queue  command_queue ,
             cl_uint            num_events_in_wait_list ,
             const cl_event *   event_wait_list ,
             cl_event *         event )
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host
  // command-queue.
  //
  // CL_INVALID_CONTEXT if context associated with command_queue and
  // events in event_wait_list are not the same.
  //
  // CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and
  // num_events_in_wait_list > 0, or event_wait_list is not NULL and
  // num_events_in_wait_list is 0, or if event objects in
  // event_wait_list are not valid events.
  detail::event::validOrError(command_queue,num_events_in_wait_list,event_wait_list);

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clEnqueueMarkerWithWaitList(cl_command_queue  command_queue ,
                            cl_uint            num_events_in_wait_list ,
                            const cl_event *   event_wait_list ,
                            cl_event *         event )
{
  validOrError(command_queue,num_events_in_wait_list,event_wait_list,event);
  
  // If the list is empty it waits for all commands previously
  // enqueued in command_queue to complete before it completes.
  xocl::ptr<xocl::event> uevent;
  if (!num_events_in_wait_list) {
    auto wait_range = xocl::xocl(command_queue)->get_event_range();
    std::vector<cl_event> ewl(wait_range.begin(),wait_range.end());
    uevent = xocl::create_hard_event(command_queue,CL_COMMAND_MARKER,ewl.size(),ewl.data());
  }
  else {
    uevent = xocl::create_hard_event(command_queue,CL_COMMAND_MARKER,num_events_in_wait_list,event_wait_list);
  }
  uevent->queue();
  xocl::assign(event,uevent.get());
  return CL_SUCCESS;
}

} // xocl

cl_int
clEnqueueMarkerWithWaitList(cl_command_queue  command_queue ,
                            cl_uint            num_events_in_wait_list ,
                            const cl_event *   event_wait_list ,
                            cl_event *         event )
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueMarkerWithWaitList
      (command_queue,num_events_in_wait_list,event_wait_list,event);
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



