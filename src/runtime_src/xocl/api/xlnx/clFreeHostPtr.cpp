/**
 * Copyright (C) 2018-2019 Xilinx, Inc
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

// Copyright 2018 Xilinx, Inc. All rights reserved.
#include <CL/opencl.h>
#include "xocl/core/error.h"
#include "xocl/core/device.h"

namespace xocl {

static void
validOrError(cl_device_id    device,
	     void*               ptr,
	     cl_int*             errcode_ret)

{
}

static void
clFreeHostPtr(cl_device_id   device,
	      void*               ptr,
	      cl_int*             errcode_ret)
{
  validOrError(device,ptr,errcode_ret);
  xocl::xocl(device)->freeHostPtr(ptr);
}

} //xocl

CL_API_ENTRY void CL_API_CALL
clFreeHostPtr(cl_device_id   device,
	      void*               ptr,
	      cl_int*             errcode_ret) CL_API_SUFFIX__VERSION_1_0
{
  try {
//    PROFILE_LOG_FUNCTION_CALL;
    xocl::clFreeHostPtr
      (device,ptr,errcode_ret);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,ex.get_code());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    xocl::assign(errcode_ret,CL_INVALID_VALUE);
  }
}

