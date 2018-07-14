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
#include "xocl/core/event.h"
#include "detail/event.h"
#include "plugin/xdp/profile.h"

namespace xocl {

static void
validOrError(cl_event event)
{
  if (!config::api_checks())
    return;

  detail::event::validOrError(event);
}

static cl_int
clReleaseEvent(cl_event event)
{
  validOrError(event);

  if (!xocl(event)->release())
    return CL_SUCCESS;

  // If this event has no references then it must be complete
  // otherwise it would still be in a command queue something is wrong
  auto status = xocl(event)->get_status();
  if (status!=CL_COMPLETE)
    throw xocl::error(CL_INVALID_EVENT,"Event '" + xocl(event)->get_suid() + "' is unreferenced but not complete");

  delete xocl(event);

  return CL_SUCCESS;
}

namespace api {

cl_int
clReleaseEvent(cl_event event)
{
  return ::xocl::clReleaseEvent(event);
}

} // api

} // xocl

cl_int
clReleaseEvent(cl_event event)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clReleaseEvent(event);
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


