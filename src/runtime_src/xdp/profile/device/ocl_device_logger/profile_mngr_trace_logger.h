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

#ifndef _XDP_PROFILE_DEVICE_TRACE_LOGGER_USING_PROFILE_MNGR_H
#define _XDP_PROFILE_DEVICE_TRACE_LOGGER_USING_PROFILE_MNGR_H

#include "xdp/config.h"
#include "xdp/profile/device/device_trace_logger.h"

namespace xdp {

class RTProfile;

class TraceLoggerUsingProfileMngr : public DeviceTraceLogger
{
  RTProfile* profileMngr;
  std::string deviceName;
  std::string binaryName;

public:

  XDP_EXPORT
  TraceLoggerUsingProfileMngr(RTProfile* profMgr, std::string devName, std::string binary);
  XDP_EXPORT
  virtual ~TraceLoggerUsingProfileMngr();

  XDP_EXPORT
  virtual void processTraceData(xclTraceResultsVector& traceVector);
  XDP_EXPORT
  virtual void endProcessTraceData(xclTraceResultsVector& traceVector);

  const std::string& getDeviceName() { return deviceName; } 
};

}
#endif
