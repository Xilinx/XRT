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

#include "xocl/core/error.h"
#include "xocl/core/device.h"
#include "detail/device.h"
#include "profile.h"

namespace xocl {

static void
validOrError(cl_device_id device)
{
  if(!config::api_checks())
    return;

  detail::device::validOrError(device);
}

static cl_int 
clReleaseDevice(cl_device_id device)
{
  validOrError(device);
  if (!xocl(device)->is_sub_device())
    // platform is sole owner of root devices
    return CL_SUCCESS;
  
  // sub device is reference counted
  if (!xocl(device)->release())
    return CL_SUCCESS;
 
  delete xocl(device);
  return CL_SUCCESS;
}

}

cl_int
clReleaseDevice(cl_device_id device)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clReleaseDevice(device);
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


