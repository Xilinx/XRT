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
#include "xocl/core/memory.h"
#include "detail/memory.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"

#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_mem memobj)
{
  if (!config::api_checks())
    return;

  detail::memory::validOrError(memobj);
}

static cl_int
clRetainMemObject(cl_mem memobj)
{
  validOrError(memobj);
  xocl(memobj)->retain();
  return CL_SUCCESS;
}

} // xocl

cl_int
clRetainMemObject(cl_mem memobj)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clRetainMemObject(memobj);
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
