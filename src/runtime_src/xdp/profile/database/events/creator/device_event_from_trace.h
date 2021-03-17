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

// For the Trace results vector
#include "xclperf.h"
#include<vector>
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/device_events.h"

namespace xdp {

class DeviceEventCreatorFromTrace
{
  uint64_t deviceId = 0;
  XclbinInfo* xclbin = nullptr ;
  VPDatabase* db = nullptr;

  std::vector<uint64_t>  traceIDs;
  std::vector<std::list<VTFDeviceEvent*>> cuStarts;

  // Last Transactions
  std::vector<uint64_t> amLastTrans;
  std::vector<uint64_t> aimLastTrans;
  std::vector<uint64_t> asmLastTrans;

#if 0
  std::map<uint64_t, uint64_t> traceIDMap; // revisit
  std::map<uint64_t, std::list<uint64_t>> cuStarts;

  // last transactions
  std::map<uint64_t, uint64_t> amLastTran;
  std::map<uint64_t, uint64_t> aimLastTran;
  std::map<uint64_t, uint64_t> asmLastTran;
#endif

  double clockTrainOffset;
  double traceClockRateMHz;
  double clockTrainSlope;

  void trainDeviceHostTimestamps(uint64_t deviceTimestamp, uint64_t hostTimestamp);
  double convertDeviceToHostTimestamp(uint64_t deviceTimestamp);

  // Helper functions
  void addAIMEvent(xclTraceResults& trace, double hostTimestamp) ;

  void addKernelDataTransferEvent(VTFEventType ty, xclTraceResults& trace, uint32_t slot, int32_t cuId, double hostTimestamp) ;

  public :
  XDP_EXPORT DeviceEventCreatorFromTrace(uint64_t devId);
  ~DeviceEventCreatorFromTrace() {}

  XDP_EXPORT void createDeviceEvents(xclTraceResultsVector& traceVector);
  XDP_EXPORT void end();
};

}
#endif

