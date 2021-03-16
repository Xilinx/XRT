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

#include "detail/event.h"
#include "plugin/xdp/profile_v2.h"

namespace xocl {

static void
validOrError(cl_event event, 
             cl_int   command_exec_callback_type,
             void     (CL_CALLBACK*  pfn_event_notify )(cl_event, cl_int, void *))
{
  if(!config::api_checks())
    return;

  detail::event::validOrError(event);
  if (pfn_event_notify==NULL)
    throw xocl::error(CL_INVALID_VALUE,"clSetEventCallback function is null");
  
  if (command_exec_callback_type!=CL_COMPLETE
      && command_exec_callback_type!=CL_SUBMITTED
      && command_exec_callback_type!=CL_RUNNING)
    throw xocl::error(CL_INVALID_VALUE,"clSetEventCallback invalid callback type");
}

static cl_int
clSetEventCallback(cl_event event ,
                   cl_int command_exec_callback_type ,
                   void (CL_CALLBACK*  pfn_event_notify )(cl_event, cl_int, void *),
                   void *user_data )
{
  validOrError(event,command_exec_callback_type,pfn_event_notify);
  // Check if event is already complete.  This call is redundant
  // because add_callback makes the same check, but it avoids 
  // creating the callback function object so is slightly cheaper.
  // Note that add_callback *must* make the check because status of
  // event can change after below check but before add_callback is 
  // called.
  if (xocl::xocl(event)->get_status()==CL_COMPLETE)
    pfn_event_notify(event,CL_COMPLETE,user_data);
  else
    xocl::xocl(event)->add_callback([=](cl_int status) { pfn_event_notify(event,status,user_data); });
  return CL_SUCCESS;
}

namespace api {

cl_int
clSetEventCallback(cl_event event ,
                   cl_int   command_exec_callback_type ,
                   void     (CL_CALLBACK *  pfn_event_notify )(cl_event, cl_int, void *),
                   void *   user_data)
{
  // No profile log, used internally in api implementations
  return ::xocl::clSetEventCallback
    (event,command_exec_callback_type,pfn_event_notify,user_data);
}

} // api

} // xocl

cl_int
clSetEventCallback( cl_event     event ,
                    cl_int       command_exec_callback_type ,
                    void (CL_CALLBACK *  pfn_event_notify )(cl_event, cl_int, void *),
                    void *       user_data )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return ::xocl::clSetEventCallback
      (event,command_exec_callback_type,pfn_event_notify,user_data);
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


