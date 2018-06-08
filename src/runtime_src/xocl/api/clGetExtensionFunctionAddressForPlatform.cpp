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

#include <CL/cl.h>
#include "detail/platform.h"
#include "profile.h"

namespace xocl {

static void
validOrError(cl_platform_id platform, const char* func_name)
{
  if (!config::api_checks())
    return;

  detail::platform::validOrError(platform);
}

static void*
clGetExtensionFunctionAddressForPlatform(cl_platform_id platform,
                                         const char *   func_name)
{
  validOrError(platform,func_name);
  return nullptr;
}

} // namespace xocl

void*
clGetExtensionFunctionAddressForPlatform(cl_platform_id platform ,
                                         const char *   func_name)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clGetExtensionFunctionAddressForPlatform(platform,func_name);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
  }
  return nullptr;
}


