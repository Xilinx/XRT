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

// Copyright 2017-2020 Xilinx, Inc. All rights reserved.

#include "xocl/config.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/memory.h"
#include "xocl/core/event.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"
#include "enqueue.h"
#include "detail/command_queue.h"
#include "detail/memory.h"
#include "detail/event.h"
#include "detail/context.h"
#include "plugin/xdp/appdebug.h"
#include "plugin/xdp/profile_v2.h"

namespace xocl {

static void
validOrError(cl_command_queue   command_queue,
             cl_mem             buffer,
             cl_bool            blocking,
             size_t             offset,
             size_t             size,
             const void *       ptr,
             cl_uint            num_events_in_wait_list ,
             const cl_event *   event_wait_list ,
             cl_event *         event_parameter)
{
  if (!config::api_checks())
    return;

  if (!ptr)
    throw error(CL_INVALID_VALUE,"ptr == nullptr");

  detail::command_queue::validOrError(command_queue);
  if (xocl(buffer)->get_flags() & ~CL_MEM_REGISTER_MAP)
    detail::memory::validOrError(buffer,offset,size);
  detail::event::validOrError(command_queue,num_events_in_wait_list,event_wait_list,blocking/*check status*/);
  detail::context::validOrError(xocl(command_queue)->get_context(),{buffer});

  if((xocl::xocl(buffer)->get_flags() & CL_MEM_HOST_READ_ONLY) ||
     (xocl::xocl(buffer)->get_flags() & CL_MEM_HOST_NO_ACCESS))
    throw xocl::error(CL_INVALID_OPERATION,"buffer flags do not allow writing");

  // CL_INVALID_OPERATION if CL_MEM_REGISTER_MAP and not a blocking read
  if ((xocl(buffer)->get_flags() & CL_MEM_REGISTER_MAP) && !blocking)
    throw error(CL_INVALID_OPERATION,"CL_MEM_REGISTER_MAP requires blocking write");

  // CL_INVALID_OPERATION if CL_MEM_REGISTER_MAP and not a multiple of 4 bytes
  if ((xocl(buffer)->get_flags() & CL_MEM_REGISTER_MAP) && (size%4))
    throw error(CL_INVALID_OPERATION,"CL_MEM_REGISTER_MAP requires size multiple of 4 bytes");
}

static cl_int
clEnqueueWriteBuffer(cl_command_queue   command_queue,
                     cl_mem             buffer,
                     cl_bool            blocking,
                     size_t             offset,
                     size_t             size,
                     const void *       ptr,
                     cl_uint            num_events_in_wait_list ,
                     const cl_event *   event_wait_list ,
                     cl_event *         event_parameter)
{
  validOrError(command_queue,buffer,blocking,offset,size,ptr,num_events_in_wait_list,event_wait_list,event_parameter);

  // xlnx extension
  if (xocl(buffer)->get_flags() & CL_MEM_REGISTER_MAP) {
    auto context = xocl(command_queue)->get_context();
    auto uevent = xocl::create_soft_event(context,CL_COMMAND_WRITE_BUFFER,num_events_in_wait_list,event_wait_list);
    // queue the event, block until successfully submitted
    uevent->queue(true/*wait*/);
    auto device = xocl::xocl(command_queue)->get_device();
    device->write_register(xocl(buffer),offset,ptr,size);
    uevent->set_status(CL_COMPLETE);
    xocl::assign(event_parameter,uevent.get());
    return CL_SUCCESS;
  }

  auto uevent = xocl::create_hard_event
    (command_queue,CL_COMMAND_WRITE_BUFFER,num_events_in_wait_list,event_wait_list);
  xocl::enqueue::set_event_action(uevent.get(),xocl::enqueue::action_write_buffer,buffer,offset,size,ptr);
  xocl::profile::set_event_action(uevent.get(), xocl::profile::action_write, buffer);
  xocl::profile::counters::set_event_action(uevent.get(), xocl::profile::counter_action_write, buffer) ;
#ifndef _WIN32
  xocl::lop::set_event_action(uevent.get(), xocl::lop::action_write);
#endif
  xocl::appdebug::set_event_action(uevent.get(),xocl::appdebug::action_readwrite,buffer,offset,size,ptr);
 
  uevent->queue();
  if (blocking)
    uevent->wait();

  xocl::assign(event_parameter,uevent.get());
  return CL_SUCCESS;
}

namespace api {

cl_int
clEnqueueWriteBuffer(cl_command_queue   command_queue,
                     cl_mem             buffer,
                     cl_bool            blocking,
                     size_t             offset,
                     size_t             size,
                     const void *       ptr,
                     cl_uint            num_events_in_wait_list ,
                     const cl_event *   event_wait_list ,
                     cl_event *         event_parameter)
{
  return ::xocl::clEnqueueWriteBuffer
    (command_queue,buffer,blocking,offset,size,ptr,num_events_in_wait_list,event_wait_list,event_parameter);
}

} // api

} // xocl

cl_int
clEnqueueWriteBuffer(cl_command_queue   command_queue,
                     cl_mem             buffer,
                     cl_bool            blocking,
                     size_t             offset,
                     size_t             size,
                     const void *       ptr,
                     cl_uint            num_events_in_wait_list ,
                     const cl_event *   event_wait_list ,
                     cl_event *         event_parameter)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    LOP_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueWriteBuffer
      (command_queue,buffer,blocking,offset,size,ptr,num_events_in_wait_list,event_wait_list,event_parameter);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
