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
#include "xocl/config.h"
#include "detail/platform.h"
#include "xocl/core/platform.h"
#include "plugin/xdp/profile.h"
#include "plugin/xdp/lop.h"
#include <CL/cl_ext_xilinx.h>
#include <map>

namespace xocl {

static const std::map<const std::string, void *> extensionFunctionTable = {
  std::pair<const std::string, void *>("clCreateStream", (void *)clCreateStream),
  std::pair<const std::string, void *>("clReleaseStream", (void *)clReleaseStream),
  std::pair<const std::string, void *>("clWriteStream", (void *)clWriteStream),
  std::pair<const std::string, void *>("clReadStream", (void *)clReadStream),
  std::pair<const std::string, void *>("clCreateStreamBuffer", (void *)clCreateStreamBuffer),
  std::pair<const std::string, void *>("clReleaseStreamBuffer", (void *)clReleaseStreamBuffer),
  std::pair<const std::string, void *>("clPollStreams", (void *)clPollStreams),
  std::pair<const std::string, void *>("clPollStream", (void *)clPollStream),
  std::pair<const std::string, void *>("clSetStreamOpt", (void *)clSetStreamOpt),
  std::pair<const std::string, void *>("xclGetMemObjectFd", (void *)xclGetMemObjectFd),
  std::pair<const std::string, void *>("xclGetMemObjectFromFd", (void *)xclGetMemObjectFromFd),
  std::pair<const std::string, void *>("xclGetXrtDevice", (void *)xclGetXrtDevice),
  std::pair<const std::string, void *>("xclGetMemObjDeviceAddress", (void *)xclGetMemObjDeviceAddress),
  std::pair<const std::string, void *>("xclGetComputeUnitInfo", (void *)xclGetComputeUnitInfo),
  std::pair<const std::string, void *>("clIcdGetPlatformIDsKHR", (void *)clIcdGetPlatformIDsKHR),
};


static void
validOrError(cl_platform_id platform, const char* func_name)
{
  if (!config::api_checks())
    return;

  detail::platform::validOrError(platform);
  if (!func_name)
    throw error(CL_INVALID_VALUE,"func_name is nullptr");
}

static void*
clGetExtensionFunctionAddressForPlatform(cl_platform_id platform,
                                         const char *   func_name)
{
  validOrError(platform,func_name);
  if (get_global_platform() != platform)
    return nullptr;

  auto iter = extensionFunctionTable.find(func_name);
  return (iter == extensionFunctionTable.end()) ? nullptr : iter->second;
}

} // namespace xocl

void*
clGetExtensionFunctionAddressForPlatform(cl_platform_id platform ,
                                         const char *   func_name)
{
  try {
    PROFILE_LOG_FUNCTION_CALL;
    LOP_LOG_FUNCTION_CALL;
    return xocl::clGetExtensionFunctionAddressForPlatform(platform,func_name);
  }
  catch (const xrt_xocl::error& ex) {
    xocl::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xocl::send_exception_message(ex.what());
  }
  return nullptr;
}
