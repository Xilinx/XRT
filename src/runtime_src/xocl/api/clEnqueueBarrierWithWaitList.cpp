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

#include "profile.h"
#include "appdebug.h"

// Enqueues a barrier command which waits for either a list of events
// to complete, or if the list is empty it waits for all commands
// previously enqueued in command_queue to complete before it
// completes. This command blocks command execution, that is, any
// following commands enqueued after it do not execute until it
// completes. This command returns an event which can be waited on,
// i.e. this event can be waited on to insure that all events either
// in the event_wait_list or all previously enqueued commands, queued
// before this command to command_queue, have completed.

namespace xocl {

static void 
validOrError(cl_command_queue  command_queue ,
             cl_uint            num_events_in_wait_list ,
             const cl_event *   event_wait_list ,
             cl_event *         event_parameter )
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host command queue.
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
clEnqueueBarrierWithWaitList(cl_command_queue  command_queue ,
                             cl_uint            num_events_in_wait_list ,
                             const cl_event *   event_wait_list ,
                             cl_event *         event_parameter )
{
  validOrError(command_queue,num_events_in_wait_list,event_wait_list,event_parameter);
  
  // If the list is empty it waits for all commands previously
  // enqueued in command_queue to complete before it completes.
  xocl::ptr<xocl::event> uevent;
  if (!num_events_in_wait_list) {
    auto wait_range = xocl::xocl(command_queue)->get_event_range();
    std::vector<cl_event> ewl(wait_range.begin(),wait_range.end());
    uevent = xocl::create_hard_event(command_queue,CL_COMMAND_BARRIER,ewl.size(),ewl.data());
  }
  else {
    uevent = xocl::create_hard_event(command_queue,CL_COMMAND_BARRIER,num_events_in_wait_list,event_wait_list);
  }
  appdebug::set_event_action(uevent.get(),appdebug::action_barrier_marker, (int)num_events_in_wait_list,event_wait_list);

  uevent->queue();
  cl_event event = uevent.get();
  xocl::assign(event_parameter,event);
  return CL_SUCCESS;
}

namespace api {

cl_int
clEnqueueBarrierWithWaitList(cl_command_queue  command_queue ,
                             cl_uint            num_events_in_wait_list ,
                             const cl_event *   event_wait_list ,
                             cl_event *         event_parameter )
{
  return ::xocl::clEnqueueBarrierWithWaitList
      (command_queue,num_events_in_wait_list,event_wait_list,event_parameter);
}

} // api

} // xocl

cl_int
clEnqueueBarrierWithWaitList(cl_command_queue  command_queue ,
                             cl_uint            num_events_in_wait_list ,
                             const cl_event *   event_wait_list ,
                             cl_event *         event_parameter )
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueBarrierWithWaitList
      (command_queue,num_events_in_wait_list,event_wait_list,event_parameter);
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


