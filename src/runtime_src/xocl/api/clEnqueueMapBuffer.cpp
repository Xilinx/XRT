/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

//Copyright 2017-2020 Xilinx, Inc. All rights reserved.

#include "xocl/config.h"
#include "xocl/core/object.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/memory.h"
#include "detail/validate.h"

#include "enqueue.h"
#include <iostream>
#include "plugin/xdp/appdebug.h"
#include "plugin/xdp/profile_v2.h"

namespace xocl {

static void
validOrError(cl_command_queue command_queue,
             cl_mem           buffer,
             cl_map_flags     map_flags,
             size_t           offset,
             size_t           size,
             cl_uint          num_events_in_wait_list,
             const cl_event * event_wait_list)
{
  if(!config::api_checks())
    return;

  detail::command_queue::validOrError(command_queue);
  detail::memory::validOrError(buffer,map_flags,offset,size);
  detail::event::validOrError(command_queue,num_events_in_wait_list,event_wait_list);

  if ((xocl(buffer)->get_flags() & CL_MEM_WRITE_ONLY) && map_flags == CL_MAP_WRITE)
    throw error(CL_MAP_FAILURE,"Map CL_MEM_WRITE_ONLY buffer for write is undefined");

  auto ctx1 = xocl(command_queue)->get_context();
  if (ctx1 != xocl(buffer)->get_context())
    throw error(CL_INVALID_CONTEXT,"context of objects do not match");
  if (num_events_in_wait_list && ctx1 != xocl(event_wait_list[0])->get_context())
    throw error(CL_INVALID_CONTEXT,"context of objects do not match");
}

static void*
clEnqueueMapBuffer(cl_command_queue command_queue,
                   cl_mem           buffer,
                   cl_bool          blocking_map,
                   cl_map_flags     map_flags,
                   size_t           offset,
                   size_t           size,
                   cl_uint          num_events_in_wait_list,
                   const cl_event * event_wait_list,
                   cl_event *       event_parameter,
                   cl_int *         errcode_ret)
{
  validOrError(command_queue,buffer,map_flags,offset,size,num_events_in_wait_list,event_wait_list);

  auto uevent = create_hard_event(command_queue,CL_COMMAND_MAP_BUFFER,num_events_in_wait_list,event_wait_list);

  void* result = nullptr;
  enqueue::set_event_action(uevent.get(),enqueue::action_map_buffer,uevent.get(),buffer,map_flags,offset,size,&result);
  profile::set_event_action(uevent.get(), profile::action_map, buffer, map_flags);
  xocl::profile::counters::set_event_action(uevent.get(), xocl::profile::counter_action_map, buffer, map_flags);
  xocl::appdebug::set_event_action(uevent.get(),xocl::appdebug::action_map,buffer,map_flags);

  uevent->queue();
  if (blocking_map)
    uevent->wait();

  xocl::assign(event_parameter,uevent.get());
  xocl::assign(errcode_ret,CL_SUCCESS);

  return result;
}

}; // xocl

void*
clEnqueueMapBuffer(cl_command_queue command_queue,
                   cl_mem           buffer,
                   cl_bool          blocking_map,
                   cl_map_flags     map_flags,
                   size_t           offset,
                   size_t           size,
                   cl_uint          num_events_in_wait_list,
                   const cl_event * event_wait_list,
                   cl_event *       event_parameter,
                   cl_int *         errcode_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    LOP_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::
      clEnqueueMapBuffer
      (command_queue,buffer,blocking_map,map_flags,offset,size,
       num_events_in_wait_list,event_wait_list, event_parameter,errcode_ret);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
    return nullptr;
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_OUT_OF_HOST_MEMORY);
    return nullptr;
  }
}
