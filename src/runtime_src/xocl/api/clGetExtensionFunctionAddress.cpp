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

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS

#include <CL/opencl.h>
#include "xocl/config.h"
#include "xocl/core/error.h"

#include "plugin/xdp/profile.h"

#include <string>
#include <map>

namespace xocl {

static const std::map<std::string, void *> extensionFunctionTable = {
  std::pair<std::string, void *>("clCreateStream", (void *)clCreateStream),
  std::pair<std::string, void *>("clReleaseStream", (void *)clReleaseStream),
  std::pair<std::string, void *>("clWriteStream", (void *)clWriteStream),
  std::pair<std::string, void *>("clReadStream", (void *)clReadStream),
  std::pair<std::string, void *>("clCreateStreamBuffer", (void *)clCreateStreamBuffer),
  std::pair<std::string, void *>("clReleaseStreamBuffer", (void *)clReleaseStreamBuffer),
  std::pair<std::string, void *>("clPollStreams", (void *)clPollStreams),
  std::pair<std::string, void *>("clIcdGetPlatformIDsKHR", (void *)clIcdGetPlatformIDsKHR),
  std::pair<std::string, void *>("xclGetMemObjectFd", (void *)xclGetMemObjectFd),
  std::pair<std::string, void *>("xclGetMemObjectFromFd", (void *)xclGetMemObjectFromFd),
};

static void
validOrError(const char* func_name)
{
  if (!config::api_checks())
    return;

  if (!func_name)
    throw error(CL_INVALID_VALUE,"func_name is nullptr");
}

static void*
clGetExtensionFunctionAddress(const char *func_name)
{
  validOrError(func_name);

  auto iter = extensionFunctionTable.find(func_name);
  return (iter == extensionFunctionTable.end()) ? nullptr : iter->second;
}

} // xocl

void*
clGetExtensionFunctionAddress(const char *func_name)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    return xocl::clGetExtensionFunctionAddress(func_name);
  }
  catch (const xrt::error& ex) {
    xocl::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
  }
  return nullptr;
}
