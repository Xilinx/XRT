/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef xrt_device_halops2_h
#define xrt_device_halops2_h

#include "xrt.h"
#include <string>

#include "core/include/xdp/common.h"
#include "core/include/xdp/counters.h"
#include "core/include/xdp/trace.h"
#include "core/include/xdp/app_debug.h"

#ifndef _WIN32
# include <sys/mman.h>
#endif

/**
 * This file provides a C++ API into a HAL user shim C library.
 *
 * At most one function with a particular name can have "C" linkage.
 * This means that xclhal1.h and xclhal2.h cannot be included in a
 * single compilation unit.  Alas, this header cannot be included
 * along with another header for a different HAL library.
 */

namespace xrt_xocl { namespace hal2 {

/* TBD */
typedef xclVerbosityLevel     verbosity_level;
typedef xclDeviceHandle       device_handle;
typedef xclDeviceInfo2        device_info;

class operations
{
public:
  operations(const std::string &fileName, void *fileHandle, unsigned int count);
  ~operations();

private:

  typedef size_t (* getDeviceTimeFuncType)(xclDeviceHandle handle);
  typedef size_t (* readCountersFuncType)(xclDeviceHandle handle, xdp::MonitorType type,
                                          xdp::CounterResults& counterResults);
  typedef size_t (* debugReadIPStatusFuncType)(xclDeviceHandle handle, xclDebugReadType type,
                                               void* debugResults);
  typedef size_t (* readTraceFuncType)(xclDeviceHandle handle, xdp::MonitorType type,
                                       xdp::TraceEventsVector& traceVector);
  typedef int (*xclGetSysfsPathFuncType)(xclDeviceHandle handle, const char* subdev, const char* entry, char* sysfsPath, size_t size);

private:
  const std::string mFileName;
  const void *mDriverHandle;
  const unsigned int mDeviceCount;

public:
  getDeviceTimeFuncType mGetDeviceTime;
  readCountersFuncType mReadCounters;
  readTraceFuncType mReadTrace;
  debugReadIPStatusFuncType mDebugReadIPStatus;
  xclGetSysfsPathFuncType mGetSysfsPath;

  const std::string&
  getFileName() const
  {
    return mFileName;
  }

  unsigned int
  getDeviceCount() const
  {
    return mDeviceCount;
  }


};

}} // hal2,xrt

#endif
