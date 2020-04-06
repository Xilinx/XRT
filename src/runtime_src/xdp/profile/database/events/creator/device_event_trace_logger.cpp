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

#include "xdp/profile/database/events/creator/device_event_trace_logger.h"

namespace xdp {

TraceLoggerCreatingDeviceEvents::TraceLoggerCreatingDeviceEvents(uint64_t devId)
           : DeviceTraceLogger()
{
  deviceEventCreator = new DeviceEventCreatorFromTrace(devId);
}

TraceLoggerCreatingDeviceEvents::~TraceLoggerCreatingDeviceEvents()
{
  delete deviceEventCreator;
}

void TraceLoggerCreatingDeviceEvents::processTraceData(xclTraceResultsVector& traceVector)
{
  deviceEventCreator->createDeviceEvents(traceVector);
}

void TraceLoggerCreatingDeviceEvents::endProcessTraceData(xclTraceResultsVector& traceVector)
{
  (void)traceVector;
  deviceEventCreator->end();
}

}
