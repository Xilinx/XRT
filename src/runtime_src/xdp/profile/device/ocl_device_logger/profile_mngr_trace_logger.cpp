/**
 * Copyright (C) 2020 Xilinx, Inc
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

#define XDP_SOURCE

#include "xdp/profile/core/rt_profile.h"
#include "xdp/profile/device/ocl_device_logger/profile_mngr_trace_logger.h"
 
namespace xdp {

TraceLoggerUsingProfileMngr::TraceLoggerUsingProfileMngr(RTProfile* profMgr,
                                  std::string devName, std::string binary)
        : DeviceTraceLogger(),
          profileMngr(profMgr),
          deviceName(devName),
          binaryName(binary)
{
}

TraceLoggerUsingProfileMngr::~TraceLoggerUsingProfileMngr()
{}

void TraceLoggerUsingProfileMngr::processTraceData(xclTraceResultsVector& traceVector)
{
  profileMngr->logDeviceTrace(deviceName, binaryName, XCL_PERF_MON_MEMORY, traceVector, false);
}

void TraceLoggerUsingProfileMngr::endProcessTraceData(xclTraceResultsVector& traceVector)
{
  profileMngr->logDeviceTrace(deviceName, binaryName, XCL_PERF_MON_MEMORY, traceVector, true);
}

}
