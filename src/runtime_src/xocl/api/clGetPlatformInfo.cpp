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

#include "xocl/core/param.h"
#include "xocl/core/error.h"
#include "xocl/core/platform.h"
#include "detail/platform.h"

#include "profile.h"
#include "xocl/config.h"

namespace xocl {

static void
validOrError(cl_platform_id   platform,
             cl_platform_info param_name,
             size_t           param_value_size,
             void *           param_value,
             size_t *         param_value_size_ret)
{
  if (!config::api_checks())
   return;

  detail::platform::validOrError(platform);
}

static cl_int 
clGetPlatformInfo(cl_platform_id   platform,
                  cl_platform_info param_name,
                  size_t           param_value_size,
                  void *           param_value,
                  size_t *         param_value_size_ret)
{
  // The platform argument can be null, and behavior is implementation
  // defined.  Here we simply use the global platform
  if (!platform && !(platform=xocl::get_global_platform()))
    throw xocl::error(CL_INVALID_PLATFORM,"clGetPlatformInfo");

  validOrError(platform,param_name,param_value_size,param_value,param_value_size_ret);

  xocl::param_buffer buffer { param_value, param_value_size, param_value_size_ret };
  auto xplatform = xocl::xocl(platform);

  switch(param_name) {
  case CL_PLATFORM_PROFILE :
    buffer.as<char>() = "EMBEDDED_PROFILE";
    break;
  case CL_PLATFORM_VERSION :
    buffer.as<char>() = "OpenCL 1.0";
    break;
  case CL_PLATFORM_NAME :
    buffer.as<char>() = "Xilinx";
    break;
  case CL_PLATFORM_VENDOR :
    buffer.as<char>() = "Xilinx";
    break;
  case CL_PLATFORM_EXTENSIONS :
    buffer.as<char>() = "cl_khr_icd";
    break;
  case CL_PLATFORM_ICD_SUFFIX_KHR :
    buffer.as<char>() = "";
    break;
  default:
    return CL_INVALID_VALUE;
    break;
  }

  return CL_SUCCESS;
}

} // xocl

cl_int
clGetPlatformInfo(cl_platform_id   platform,
                  cl_platform_info param_name,
                  size_t           param_value_size,
                  void *           param_value,
                  size_t *         param_value_size_ret)
{
  try {
    PROFILE_LOG_FUNCTION_CALL
      return xocl::clGetPlatformInfo
      (platform, param_name, param_value_size, param_value, param_value_size_ret);
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


