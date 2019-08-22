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
#include "xocl/core/platform.h"

#include "detail/platform.h"

#include "plugin/xdp/profile.h"

namespace xocl {

static void
validOrError(cl_uint          num_entries,
             cl_platform_id * platforms,
             cl_uint *        num_platforms)
{
  if (!config::api_checks())
    return;

  detail::platform::validOrError(num_entries,platforms);
}

static cl_int
clGetPlatformIDs(cl_uint          num_entries,
                 cl_platform_id * platforms,
                 cl_uint *        num_platforms)
{
  validOrError(num_entries,platforms,num_platforms);

  auto platform = get_global_platform();
  if (num_entries && platforms)
    platforms[0] = platform;
  if (num_platforms)
    *num_platforms = platform ? 1 : 0;
  return CL_SUCCESS;
}

namespace api {

cl_int
clGetPlatformIDs(cl_uint          num_entries,
                 cl_platform_id * platforms,
                 cl_uint *        num_platforms)
{
  return ::xocl::clGetPlatformIDs(num_entries, platforms, num_platforms);
}

} // api

} // xocl

CL_API_ENTRY cl_int CL_API_CALL
clGetPlatformIDs(cl_uint          num_entries,
                 cl_platform_id * platforms,
                 cl_uint *        num_platforms) CL_API_SUFFIX__VERSION_1_0
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clGetPlatformIDs
      (num_entries, platforms, num_platforms);
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

cl_int
clIcdGetPlatformIDsKHR(cl_uint          num_entries,
                       cl_platform_id * platforms,
                       cl_uint *        num_platforms)
{
  if (num_entries && platforms)
    platforms[0] = nullptr;

  try {
    xocl::clGetPlatformIDs
      (num_entries, platforms, num_platforms);
  }
  catch (const xocl::error& ex) {
    xocl::send_exception_message(ex.what());
    return ex.get_code();
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
    return CL_OUT_OF_HOST_MEMORY;
  }

  if (num_platforms && (*num_platforms == 0))
    return CL_PLATFORM_NOT_FOUND_KHR;
  else if (num_platforms && (*num_platforms > 0))
    return CL_SUCCESS;

  assert(num_entries && platforms);
  return platforms[0] ? CL_SUCCESS : CL_PLATFORM_NOT_FOUND_KHR;
}
