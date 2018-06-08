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

#include "event.h"
#include "context.h"
#include "command_queue.h"
#include "xocl/core/context.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/event.h"
#include "xocl/core/error.h"
#include <algorithm>
#include <cstdlib>

namespace xocl { namespace detail {

namespace event {

void 
validOrError(cl_context ctx, cl_uint num_events, const cl_event* event_list, bool check_status)
{
  if (!num_events && !event_list)
    return;

  if(!num_events && event_list)
    throw error(CL_INVALID_VALUE,"number of events is 0");
  if (num_events && !event_list)
    throw error(CL_INVALID_VALUE,"event_list is nullptr");

  static bool conformance = (std::getenv("XCL_CONFORMANCE")!=nullptr);
  // Disable this check for conformance mode since program binaries are reused across contexts
  if (!conformance) {
    auto valid_event = [ctx,check_status](cl_event ev)
    {
      validOrError(ev);

      if (xocl(ev)->get_context() != ctx)
        throw error(CL_INVALID_CONTEXT,"event context mismatch");
      
      if (check_status && xocl(ev)->get_status() < 0)
        throw error(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST,"bad status for event");
    };
    std::for_each(event_list,event_list+num_events,valid_event);
  }

}

void
validOrError(const cl_event ev)
{
  if (!ev)
    throw error(CL_INVALID_EVENT,"event is nullptr");
}

void 
validOrError(cl_command_queue cq, cl_uint num_events, const cl_event* event_list, bool check_status)
{
  command_queue::validOrError(cq);
  auto context = xocl(cq)->get_context();
  context::validOrError(context);
  validOrError(xocl(cq)->get_context(),num_events,event_list,check_status);
}

void 
validOrError(cl_uint num_events, const cl_event* event_list,bool check_status)
{
  if (!num_events && !event_list)
    return;
  validOrError(xocl(event_list[0])->get_context(),num_events,event_list,check_status);
}

} // event

}} // detail,xocl


