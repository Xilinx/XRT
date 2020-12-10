/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include "xocl/core/device.h"
#include <CL/opencl.h>

namespace xocl {

static void
validOrError(cl_stream                  stream,
	cl_streams_poll_req_completions*completions,
	cl_int                          min_num_completion,
	cl_int                          max_num_completion,
	cl_int*                         actual_num_completion,
	cl_int                          timeout,
	cl_int*                         errcode_ret)

{
  if (min_num_completion <= 0)
    throw error(CL_INVALID_VALUE,"minimum number of completion must be > 0");
}

static cl_int
clPollStream(cl_stream                 stream,
	cl_streams_poll_req_completions*completions,
	cl_int                          min,
	cl_int                          max,
	cl_int*                         actual,
	cl_int                          timeout,
	cl_int*                         errcode_ret)
{
  validOrError(stream,completions,min,max,actual,timeout,errcode_ret);
  int ret = xocl::xocl(stream)->poll_stream(completions,min,max,actual,timeout);
  xocl::assign(errcode_ret, ret);
  return ret;
}

} //xocl

CL_API_ENTRY cl_int CL_API_CALL
clPollStream(cl_stream                  stream,
       	cl_streams_poll_req_completions* completions,
	cl_int                           min_num_completion,
	cl_int                           max_num_completion,
	cl_int*                          actual_num_completion,
	cl_int                           timeout,
	cl_int * errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clPollStream
      (stream,completions,min_num_completion,max_num_completion,actual_num_completion,timeout,errcode_ret);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_INVALID_VALUE);
  }
  return CL_INVALID_VALUE;
}
