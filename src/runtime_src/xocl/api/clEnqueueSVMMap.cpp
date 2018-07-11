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

//Copyright 2017 Xilinx, Inc. All rights reserved.

#include "xocl/config.h"
#include "xocl/core/object.h"
#include "xocl/core/command_queue.h"
#include "xocl/core/memory.h"
#include "detail/validate.h"

#include "enqueue.h"
#include <iostream>
#include "xoclAppDebug.h"
#include "xoclProfile.h"

namespace xocl {

static void
validOrError(cl_command_queue command_queue,
             cl_map_flags     map_flags,
             void *           svm_ptr,
             size_t           size,
             cl_uint          num_events_in_wait_list,
             const cl_event * event_wait_list)
{
  if(!config::api_checks())
    return;

  detail::command_queue::validOrError(command_queue); 
  detail::event::validOrError(command_queue,num_events_in_wait_list,event_wait_list);

  auto ctx1 = xocl(command_queue)->get_context();
  if (num_events_in_wait_list && ctx1 != xocl(event_wait_list[0])->get_context())
    throw error(CL_INVALID_CONTEXT,"context of objects do not match");

  // CL_INVALID_VALUE if svm_ptr is NULL.
  if (!svm_ptr)
    throw error(CL_INVALID_VALUE,"SVM pointer is NULL");

  // CL_INVALID_VALUE if size is 0
  // or if values specified in map_flags are not valid.
  if (size == 0)
    throw error(CL_INVALID_VALUE,"SVM pointer is NULL");

  if ((map_flags & (CL_MAP_WRITE | CL_MAP_READ)) &&
      (map_flags & CL_MAP_WRITE_INVALIDATE_REGION))
    throw error(CL_INVALID_VALUE,"Mutually exclusive flags specified");

}

static cl_int
clEnqueueSVMMap(cl_command_queue command_queue,
                cl_bool          blocking_map, 
                cl_map_flags     map_flags,
                void *           svm_ptr,
                size_t           size,
                cl_uint          num_events_in_wait_list,
                const cl_event * event_wait_list,
                cl_event       * event)
{
  validOrError(command_queue,map_flags,svm_ptr,size,num_events_in_wait_list,event_wait_list);

  auto uevent = create_hard_event(command_queue,CL_COMMAND_SVM_MAP,num_events_in_wait_list,event_wait_list);

  enqueue::set_event_action(uevent.get(),enqueue::action_map_svm_buffer,uevent.get(),map_flags,svm_ptr,size);
  // TODO: Think about how to profile & appdebug (clEnqueueSVMUnmap don't have size argument)
  //profile::set_event_action(uevent.get(),profile::action_map_svm,buffer,map_flags,svm_ptr,size);
  //appdebug::set_event_action(uevent.get(),appdebug::action_map_svm,map_flags,svm_ptr,size);

  uevent->queue();
  if (blocking_map)
    uevent->wait();

  xocl::assign(event,uevent.get());

  return CL_SUCCESS;
}

}; // xocl

// Note that since we are enqueuing a command with a SVM buffer,
// the region is already mapped in the host address space.
// clEnqueueSVMMap, and clEnqueueSVMUnmap act as synchronization
// points for the region of the SVM buffer specified in these calls.
cl_int
clEnqueueSVMMap(cl_command_queue command_queue,
                cl_bool          blocking_map, 
                cl_map_flags     map_flags,
                void *           svm_ptr,
                size_t           size,
                cl_uint          num_events_in_wait_list,
                const cl_event * event_wait_list,
                cl_event       * event)
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::
      clEnqueueSVMMap
      (command_queue,blocking_map,map_flags,svm_ptr,size,
       num_events_in_wait_list,event_wait_list, event);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_RESOURCES;
  }
}

