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
#include "xocl/core/context.h"
#include "detail/context.h"
#include "plugin/xdp/profile.h"
#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_context context)
{
  if (!config::api_checks())
    return;

  detail::context::validOrError(context);
}

static cl_int
clRetainContext(cl_context  context )
{
  validOrError(context);
  xocl(context)->retain();
  return CL_SUCCESS;
}

} // xocl

cl_int
clRetainContext(cl_context context)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clRetainContext(context);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }
}
