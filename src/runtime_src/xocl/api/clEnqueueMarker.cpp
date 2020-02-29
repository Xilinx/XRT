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

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS

#include "xocl/config.h"
#include "xocl/core/event.h"
#include "detail/command_queue.h"

#include "plugin/xdp/profile.h"
#include "plugin/xdp/appdebug.h"
#include "plugin/xdp/lop.h"

#include <CL/opencl.h>
#include <vector>

#ifdef _WIN32
# pragma warning ( disable : 4267 )
#endif

namespace xocl {

static void
validOrError(cl_command_queue command_queue,
             cl_event*        event_parameter)
{
  if (!config::api_checks())
    return;

  detail::command_queue::validOrError(command_queue);

  if (!event_parameter)
    throw error(CL_INVALID_VALUE,"event_parameter is nullptr");
}

static cl_int
clEnqueueMarker(cl_command_queue command_queue,
                cl_event *event_parameter)
{
  validOrError(command_queue,event_parameter);

  // A marker is complete when all events ahead of it is complete, so
  // create the event with an event wait list consisting of all
  // currently queued events.
  //
  // Be very careful controlling the scope here.  It is important that
  // the current command_queue events are valid while the event is
  // constructed.  To make this possible, the event_range returned by
  // the command queue also retains a mutex lock on the command queue.
  // Since the command queue should not remained locked, we want the
  // lock (event_range) released immedidately after the event has been
  // constructed.  This interface to event/queue doesn't look quite right
  xocl::ptr<xocl::event> pevent;
  {
    auto wait_range = xocl::xocl(command_queue)->get_event_range();
    std::vector<cl_event> wait_list(wait_range.begin(),wait_range.end());
    pevent = xocl::create_hard_event
      (command_queue,CL_COMMAND_MARKER,wait_list.size(),wait_list.data());
    xocl::appdebug::set_event_action
      (pevent.get(),xocl::appdebug::action_barrier_marker,wait_list.size(),wait_list.data());
  }
  pevent->queue();
  xocl::assign(event_parameter,pevent.get());
  return CL_SUCCESS;
}

} // xocl

cl_int
clEnqueueMarker(cl_command_queue command_queue,
                cl_event*        event_parameter)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    LOP_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueMarker
      (command_queue,event_parameter);
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
