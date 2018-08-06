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
#include "xocl/core/error.h"

#include "plugin/xdp/profile.h"

namespace xocl {

static void
validOrError(cl_command_queue   command_queue ,
             void (*user_func)(void *), 
             void *             args ,
             size_t             cb_args , 
             cl_uint            num_mem_objects ,
             const cl_mem *     mem_list ,
             const void **      args_mem_loc ,
             cl_uint            num_events_in_wait_list ,
             const cl_event *   event_wait_list ,
             cl_event *         event )
{
  if (!config::api_checks())
    return;

  // CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host
  // command-queue.

  // CL_INVALID_CONTEXT if context associated with command_queue and
  // events in event_wait_list are not the same.

  // CL_INVALID_VALUE if user_func is NULL.

  // CL_INVALID_VALUE if args is a NULL value and cb_args > 0, or if
  // args is a NULL value and num_mem_objects > 0.

  // CL_INVALID_VALUE if args is not NULL and cb_args is 0.

  // CL_INVALID_VALUE if num_mem_objects > 0 and mem_list or
  // args_mem_loc are NULL.

  // CL_INVALID_VALUE if num_mem_objects = 0 and mem_list or
  // args_mem_loc are not NULL.
  
  // CL_INVALID_OPERATION if the device associated with command_queue
  // cannot execute the native kernel.

  // CL_INVALID_MEM_OBJECT if one or more memory objects specified in
  // mem_list are not valid or are not buffer objects.

  // CL_OUT_OF_RESOURCES if there is a failure to queue the execution
  // instance of kernel on the command-queue because of insufficient
  // resources needed to execute the kernel.

  // CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to
  // allocate memory for data store associated with buffer objects
  // specified as arguments to kernel.

  // CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and
  // num_events_in_wait_list > 0, or event_wait_list is not NULL and
  // num_events_in_wait_list is 0, or if event objects in
  // event_wait_list are not valid events.

  // CL_INVALID_OPERATION if SVM pointers are passed as arguments to a
  // kernel and the device does not support SVM or if system pointers
  // are passed as arguments to a kernel and/or stored inside SVM
  // allocations passed as kernel arguments and the device does not
  // support fine grain system SVM allocations.

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

cl_int
clEnqueueNativeKernel(cl_command_queue   command_queue ,
					  void (*user_func)(void *), 
                      void *             args ,
                      size_t             cb_args , 
                      cl_uint            num_mem_objects ,
                      const cl_mem *     mem_list ,
                      const void **      args_mem_loc ,
                      cl_uint            num_events_in_wait_list ,
                      const cl_event *   event_wait_list ,
                      cl_event *         event )
{
  validOrError(command_queue,user_func,args,cb_args,num_mem_objects,mem_list,args_mem_loc,
               num_events_in_wait_list,event_wait_list,event);
  throw error(CL_XILINX_UNIMPLEMENTED);
}

} // xocl

cl_int
clEnqueueNativeKernel(cl_command_queue   command_queue ,
		      void               (*user_func)(void *), 
                      void *             args ,
                      size_t             cb_args , 
                      cl_uint            num_mem_objects ,
                      const cl_mem *     mem_list ,
                      const void **      args_mem_loc ,
                      cl_uint            num_events_in_wait_list ,
                      const cl_event *   event_wait_list ,
                      cl_event *         event )
{
  try {
    PROFILE_LOG_FUNCTION_CALL_WITH_QUEUE(command_queue);
    return xocl::clEnqueueNativeKernel
      (command_queue,user_func,args,cb_args,num_mem_objects,mem_list,args_mem_loc,
       num_events_in_wait_list,event_wait_list,event);
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



