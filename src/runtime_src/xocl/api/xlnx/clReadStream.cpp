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
#include "plugin/xdp/profile_v2.h"
#include "xocl/core/device.h"
#include <CL/opencl.h>

#ifdef _WIN32
#pragma warning ( disable : 4267 )
#endif

namespace xocl {

static void
validOrError(cl_stream           stream,
	     void*               ptr,
	     size_t              size,
	     cl_stream_xfer_req* attributes,
	     cl_int*             errcode_ret)

{
}

static cl_int
clReadStream(cl_stream            stream,
	      void*                ptr,
	      size_t               size,
	      cl_stream_xfer_req*  attributes,
	      cl_int*              errcode_ret)
{
  validOrError(stream,ptr,size,attributes,errcode_ret);
  return xocl::xocl(stream)->read(ptr, size, attributes);
}

} //xocl

CL_API_ENTRY cl_int CL_API_CALL
clReadStream(cl_stream           stream,
	      void*               ptr,
	      size_t              size,
	      cl_stream_xfer_req* attributes,
	      cl_int*             errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clReadStream
      (stream,ptr,size,attributes,errcode_ret);
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
