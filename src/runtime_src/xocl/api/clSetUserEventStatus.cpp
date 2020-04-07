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
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_event event, cl_int execution_status)
{
  if (!config::api_checks())
    return;

  // CL_INVALID_EVENT if event is not a valid user event.
  detail::event::validOrError(event);

  // CL_INVALID_VALUE if the execution_status is not CL_COMPLETE or a
  // negative integer value.
  if((execution_status!=CL_COMPLETE) && (execution_status>=0))
    throw xocl::error(CL_INVALID_VALUE,"clSetUserEventStatus bad execution status");

  // CL_INVALID_OPERATION if the execution_status for event has
  // already been changed by a previous call to clSetUserEventStatus.
  //  A user event can only be changed through clSetUserEventStatus,
  //  hence if its status is different from the initial (CL_SUBMITTED)
  //  status then this function has already been called
  if (xocl::xocl(event)->get_status()!=CL_SUBMITTED)
    throw xocl::error(CL_INVALID_OPERATION,"clSetUserEventStatus event has not been submitted");

  // CL_OUT_OF_RESOURCES if there is a failure to allocate resources
  // required by the OpenCL implementation on the device.

  // CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources
  // required by the OpenCL implementation on the host.
}

static cl_int
clSetUserEventStatus(cl_event event, cl_int execution_status)
{
  validOrError(event,execution_status);
  if(execution_status==CL_COMPLETE)
    xocl::xocl(event)->set_status(CL_COMPLETE);
  else
    xocl::xocl(event)->abort(execution_status);

  return CL_SUCCESS;
}

} // xocl

cl_int
clSetUserEventStatus(cl_event    event ,
                     cl_int      execution_status )
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clSetUserEventStatus(event,execution_status);
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
