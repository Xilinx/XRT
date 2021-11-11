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

    // Parsing functions for getting different parts of a device event packet
    inline uint64_t getDeviceTimestamp(uint64_t trace)
      { return (trace & 0x1FFFFFFFFFFF) - firstTimestamp; }
    inline bool isDeviceEventTypeStart(uint64_t trace)
      { return ((trace >> 45) & 0xF) ? false : true ;  }
    inline uint64_t getEventFlags(uint64_t trace)
      { return ((trace >> 45) & 0xF) | ((trace >> 57) & 0x10) ; }
    inline uint64_t getTraceId(uint64_t trace)
      { return ((trace >> 49) & 0xFFF) ; }
    inline uint64_t getReserved(uint64_t trace)
      { return ((trace >> 61) & 0x1) ; }
    inline bool isClockTraining(uint64_t trace)
      { return (((trace >> 63) & 0x1) == 1) ;}

    double clockTrainOffset;
    double traceClockRateMHz;
    double clockTrainSlope;

    bool warnCUIncomplete=false;

    void trainDeviceHostTimestamps(uint64_t deviceTimestamp, uint64_t hostTimestamp);
    double convertDeviceToHostTimestamp(uint64_t deviceTimestamp);

    // Functions for adding device events based on the monitor type
    void addAMEvent (uint64_t trace, double hostTimestamp) ;
    void addAIMEvent(uint64_t trace, double hostTimestamp) ;
    void addASMEvent(uint64_t trace, double hostTimestamp) ;

    // Functions for adding specific types of device events from the
    //  raw device data
    void addCUEvent(uint64_t trace, double hostTimestamp,
                    uint32_t slot, uint64_t monTraceId, int32_t cuId) ;
    void addStallEvent(uint64_t trace, double hostTimestamp,
                       uint32_t slot, uint64_t monTraceId, int32_t cuId,
                       VTFEventType type, uint64_t mask) ;
    void addKernelDataTransferEvent(VTFEventType ty, uint64_t trace,
                                    uint32_t slot, int32_t cuId,
                                    double hostTimestamp) ;

    void addCUEndEvent(double hostTimestamp, uint64_t deviceTimestamp,
                       uint32_t s, int32_t cuId);

    // Functions for handling dropped device packets
    void addApproximateCUEndEvents();
    void addApproximateDataTransferEndEvents();
    void addApproximateDataTransferEndEvents(int32_t cuId);
    void addApproximateStreamEndEvents();
    void addApproximateStallEndEvents(uint64_t trace, double hostTimestamp, uint32_t slot, uint64_t monTraceId, int32_t cuId) ;

    void addApproximateDataTransferEvent(VTFEventType type, uint64_t aimTraceID, int32_t amId, int32_t cuId);
    void addApproximateStreamEndEvent(uint64_t asmIndex, uint64_t asmTraceID, VTFEventType streamEventType,
                                      int32_t cuId, int32_t  amId, uint64_t cuLastTimestamp,
                                      uint64_t &asmAppxLastTransTimeStamp, bool &unfinishedASMevents);

    uint64_t firstTimestamp = 0 ;

  public:

    XDP_EXPORT DeviceTraceLogger(uint64_t devId);
    XDP_EXPORT ~DeviceTraceLogger();

    XDP_EXPORT void processTraceData(void* data, uint64_t numBytes) ;
    XDP_EXPORT void endProcessTraceData();
  } ;

}
#endif
