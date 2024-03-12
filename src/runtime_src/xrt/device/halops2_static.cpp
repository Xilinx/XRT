/**
 * Copyright (C) 2021 Xilinx, Inc
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
#if 0
  // Profiling Functions
  mGetDeviceTime = &xclGetDeviceTimestamp;
  mReadCounters = &xclPerfMonReadCounters;
  mReadTrace = &xclPerfMonReadTrace;
  mDebugReadIPStatus = &xclDebugReadIPStatus;
  mGetSysfsPath = &xclGetSysfsPath;
#endif
}

operations::
~operations()
{
}

}} // hal2,xrt
