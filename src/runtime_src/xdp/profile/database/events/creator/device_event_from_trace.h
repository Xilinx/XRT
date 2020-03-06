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

#ifndef EVENT_CREATOR_FROM_TRACE_H
#define EVENT_CREATOR_FROM_TRACE_H
#endif

// For the Trace results vector
#include "xclperf.h"
#include<map>
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/device_events.h"

namespace xdp {

class DeviceEventCreatorFromTrace
{
  VPDatabase* db = nullptr;

  std::map<uint64_t, uint64_t> traceIDMap; // revisit

  double mTrainOffset;
  double mTraceClockRateMHz = 285; // 300 ?
  double mTrainSlope = 1000.0/mTraceClockRateMHz;

  void trainDeviceHostTimestamps(uint64_t deviceTimestamp, uint64_t hostTimestamp);
  double convertDeviceToHostTimestamp(uint64_t deviceTimestamp);

  public :
  DeviceEventCreatorFromTrace()
    : db(VPDatabase::Instance())
  {}
  virtual ~DeviceEventCreatorFromTrace() {}

  XDP_EXPORT void createDeviceEvents(uint64_t deviceId, xclTraceResultsVector& traceVector);
};

}
