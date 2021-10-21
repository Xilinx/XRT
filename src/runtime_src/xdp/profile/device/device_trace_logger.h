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

#ifndef _XDP_PROFILE_DEVICE_BASE_TRACE_LOGGER_H
#define _XDP_PROFILE_DEVICE_BASE_TRACE_LOGGER_H

#include "core/include/xclperf.h"
#include <vector>

#include "xdp/config.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/device_events.h"

namespace xdp {

  // The responsiblity of this class is to convert raw Device PL events
  //  into database events and log them into the database
  class DeviceTraceLogger
  {
 private:
    uint64_t deviceId = 0;
    XclbinInfo* xclbin = nullptr ;
    VPDatabase* db = nullptr;

    std::vector<uint64_t>  traceIDs;
    // Keep track of the event ID and device timestamp of CU starts
    std::vector<std::list<std::pair<uint64_t, uint64_t>>> cuStarts;

    // Last Transactions
    std::vector<uint64_t> amLastTrans;
    std::vector<uint64_t> aimLastTrans;
    std::vector<uint64_t> asmLastTrans;

    double clockTrainOffset;
    double traceClockRateMHz;
    double clockTrainSlope;

    bool warnCUIncomplete=false;

    void trainDeviceHostTimestamps(uint64_t deviceTimestamp, uint64_t hostTimestamp);
    double convertDeviceToHostTimestamp(uint64_t deviceTimestamp);

    // Functions for adding a specific type of device event
    void addAMEvent(xclTraceResults& trace, double hostTimestamp);
    void addAIMEvent(xclTraceResults& trace, double hostTimestamp) ;

    void addCUEvent(xclTraceResults& trace, double hostTimestamp,
                    uint32_t s, uint64_t monTraceID, int32_t cuId);
    void addStallEvent(xclTraceResults& trace, double hostTimestamp,
                       uint32_t s, uint64_t monTraceID, int32_t cuId,
                       VTFEventType type, uint64_t mask) ;
    void addKernelDataTransferEvent(VTFEventType ty, xclTraceResults& trace,
                                    uint32_t slot, int32_t cuId,
                                    double hostTimestamp) ;

    void addCUEndEvent(double hostTimestamp, uint64_t deviceTimestamp,
                       uint32_t s, int32_t cuId);

    // Functions for handling dropped device packets
    void addApproximateCUEndEvents();
    void addApproximateDataTransferEndEvents();
    void addApproximateDataTransferEndEvents(int32_t cuId);
    void addApproximateStreamEndEvents();
    void addApproximateStallEndEvents(xclTraceResults& trace, double hostTimestamp, uint32_t s, uint64_t monTraceID, int32_t cuId);

    void addApproximateDataTransferEvent(VTFEventType type, uint64_t aimTraceID, int32_t amId, int32_t cuId);
    void addApproximateStreamEndEvent(uint64_t asmIndex, uint64_t asmTraceID, VTFEventType streamEventType,
                                      int32_t cuId, int32_t  amId, uint64_t cuLastTimestamp,
                                      uint64_t &asmAppxLastTransTimeStamp, bool &unfinishedASMevents);

  public:

    XDP_EXPORT DeviceTraceLogger(uint64_t devId);
    XDP_EXPORT ~DeviceTraceLogger();

    XDP_EXPORT void processTraceData(std::vector<xclTraceResults>& traceVector);
    XDP_EXPORT void endProcessTraceData();
  } ;

}
#endif
