/**
 * Copyright (C) 2018-2020 Xilinx, Inc
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

// Copyright 2018-2020 Xilinx, Inc. All rights reserved.
#include "xocl/config.h"
#include "xocl/core/stream.h"
#include "xocl/core/error.h"
#include "plugin/xdp/profile.h"
#include <CL/opencl.h>

namespace xocl {
static void
validOrError(cl_stream_mem stream_obj)
{
}

cl_int
clReleaseStreamBuffer(cl_stream_mem stream_obj)
{
  validOrError(stream_obj);
  return CL_INVALID_VALUE;
}

} //xocl

CL_API_ENTRY cl_int CL_API_CALL
clReleaseStreamBuffer(cl_stream_mem stream_obj) CL_API_SUFFIX__VERSION_1_0
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clReleaseStreamBuffer(stream_obj);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
  }
  return CL_INVALID_VALUE;
}
