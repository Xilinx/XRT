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

#ifndef _XDP_PROFILE_TRACE_LOGGER_CREATING_DEVICE_EVENTS_H
#define _XDP_PROFILE_TRACE_LOGGER_CREATING_DEVICE_EVENTS_H

#include "xdp/config.h"
#include "xdp/profile/device/device_trace_logger.h"
#include "xdp/profile/database/events/creator/device_event_from_trace.h"

namespace xdp {

class TraceLoggerCreatingDeviceEvents : public DeviceTraceLogger
{
  DeviceEventCreatorFromTrace* deviceEventCreator = nullptr;

public:

  XDP_EXPORT
  TraceLoggerCreatingDeviceEvents(uint64_t devId);
  XDP_EXPORT
  virtual ~TraceLoggerCreatingDeviceEvents();

  XDP_EXPORT
  virtual void processTraceData(std::vector<xclTraceResults>& traceVector);
  XDP_EXPORT
  virtual void endProcessTraceData(std::vector<xclTraceResults>& traceVector);
};

}
#endif
