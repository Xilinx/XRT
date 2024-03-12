/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include "halops2.h"
#include "core/common/dlfcn.h"

namespace xrt_xocl { namespace hal2 {

operations::
operations(const std::string &fileName, void *fileHandle, unsigned int count)
  : mFileName(fileName)
  ,mDriverHandle(fileHandle)
  ,mDeviceCount(count)
  ,mGetDeviceTime(0)
  ,mReadCounters(0)
  ,mReadTrace(0)
  ,mDebugReadIPStatus(0)
  ,mGetSysfsPath(0)
{
  // Profiling Functions
  mGetDeviceTime = (getDeviceTimeFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetDeviceTimestamp");
  mReadCounters = (readCountersFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonReadCounters");
  mReadTrace = (readTraceFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclPerfMonReadTrace");
  mDebugReadIPStatus = (debugReadIPStatusFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclDebugReadIPStatus");
  mGetSysfsPath = (xclGetSysfsPathFuncType)xrt_core::dlsym(const_cast<void *>(mDriverHandle), "xclGetSysfsPath");
}

operations::
~operations()
{
  xrt_core::dlclose(const_cast<void *>(mDriverHandle));
}

}} // hal2,xrt
