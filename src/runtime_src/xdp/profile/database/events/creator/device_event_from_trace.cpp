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

#define XDP_SOURCE

#include "xdp/profile/database/events/creator/device_event_from_trace.h"

namespace xdp {

  void DeviceEventCreatorFromTrace::createDeviceEvents(uint64_t deviceId, xclTraceResultsVector& traceVector, VPDatabase* db)
  {
    // Create Device Events and log them : do what is done in TraceParser::logTrace
    if(traceVector.mLength == 0)
      return;

    uint64_t timestamp = 0;

    for(unsigned int i=0; i < traceVector.mLength; i++) {
      auto& trace = traceVector.mArray[i];
      
      timestamp = trace.Timestamp;

      // assign EVENT_TYPE

      if (trace.isClockTrain) {
        trainDeviceHostTimestamps(timestamp, trace.HostTimestamp);
      }

      uint32_t s = 0;
      bool SAMPacket = (trace.TraceID >= MIN_TRACE_ID_AM && trace.TraceID <= MAX_TRACE_ID_AM);
      bool SPMPacket = (trace.TraceID >= MIN_TRACE_ID_AIM && trace.TraceID <= MAX_TRACE_ID_AIM);
      bool SSPMPacket = (trace.TraceID >= MIN_TRACE_ID_ASM && trace.TraceID < MAX_TRACE_ID_ASM);
      if (!SAMPacket && !SPMPacket && !SSPMPacket) {
        continue;
      }
      double hostTimestamp = convertDeviceToHostTimestamp(timestamp);
      if (SAMPacket) {
        s = ((trace.TraceID - MIN_TRACE_ID_AM) / 16);
        uint32_t cuEvent       = trace.TraceID & XAM_TRACE_CU_MASK;
        uint32_t stallIntEvent = trace.TraceID & XAM_TRACE_STALL_INT_MASK;
        uint32_t stallStrEvent = trace.TraceID & XAM_TRACE_STALL_STR_MASK;
        uint32_t stallExtEvent = trace.TraceID & XAM_TRACE_STALL_EXT_MASK;

        Monitor* mon  = db->getStaticInfo().getAMonitor(deviceId, s);   
		int32_t  cuId = mon->cuIndex;
        
        if(cuEvent) {
          KernelDeviceEvent* event = nullptr;
          if (!(trace.EventFlags & XAM_TRACE_CU_MASK)) {
            // end event
            VTFEvent* e = db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID);
            if(!e) {
              continue;
            }
            event = new KernelDeviceEvent(e->getEventId(), hostTimestamp, deviceId, cuId);
            event->setDeviceTimestamp(static_cast<double>(timestamp));
            db->getDynamicInfo().addEvent(event);
          } else {
            // start event
            event = new KernelDeviceEvent(0, hostTimestamp, deviceId, cuId);
            event->setDeviceTimestamp(static_cast<double>(timestamp));
            db->getDynamicInfo().addEvent(event);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, event);
          }
        }
 
        if(stallIntEvent) {
          KernelStall* event = nullptr;
          if(traceIDMap[s] & XAM_TRACE_STALL_INT_MASK) {
            // end event
            event = new KernelStall(db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID)->getEventId(),
                             hostTimestamp, KERNEL_STALL_DATAFLOW, deviceId);
            event->setDeviceTimestamp(static_cast<double>(timestamp));
            db->getDynamicInfo().addEvent(event);
          } else {
            // start event
            event = new KernelStall(0, hostTimestamp, KERNEL_STALL_DATAFLOW, deviceId);
            event->setDeviceTimestamp(static_cast<double>(timestamp));
            db->getDynamicInfo().addEvent(event);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, event);
          }
        } 

        if(stallStrEvent) {
          KernelStall* event = nullptr;
          if(traceIDMap[s] & XAM_TRACE_STALL_STR_MASK) {
            // end event
            event = new KernelStall(db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID)->getEventId(),
                             hostTimestamp, KERNEL_STALL_PIPE, deviceId);
            event->setDeviceTimestamp(static_cast<double>(timestamp));
            db->getDynamicInfo().addEvent(event);
          } else {
            // start event
            event = new KernelStall(0, hostTimestamp, KERNEL_STALL_PIPE, deviceId);
            event->setDeviceTimestamp(static_cast<double>(timestamp));
            db->getDynamicInfo().addEvent(event);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, event);
          }
        } 
        if(stallExtEvent) {
          KernelStall* event = nullptr;
          if(traceIDMap[s] & XAM_TRACE_STALL_EXT_MASK) {
            // end event
            event = new KernelStall(db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID)->getEventId(),
                             hostTimestamp, KERNEL_STALL_EXT_MEM, deviceId);
            event->setDeviceTimestamp(static_cast<double>(timestamp));
            db->getDynamicInfo().addEvent(event);
          } else {
            // start event
            event = new KernelStall(0, hostTimestamp, KERNEL_STALL_EXT_MEM, deviceId);
            event->setDeviceTimestamp(static_cast<double>(timestamp));
            db->getDynamicInfo().addEvent(event);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, event);
          }
        }
        traceIDMap[s] ^= (trace.TraceID & 0xf);
      } // SAMPacket
      else if(SPMPacket) {
        KernelMemoryAccess* memEvent = nullptr;
        if((!trace.TraceID) & 1) { // read packet
          s = trace.TraceID/2;
        Monitor* mon = db->getStaticInfo().getAIMonitor(deviceId, s);  
(void)mon; 
          // KERNEL_READ
          if(trace.EventType == XCL_PERF_MON_START_EVENT) {
            memEvent = new KernelMemoryAccess(0, hostTimestamp, KERNEL_READ, deviceId);
            memEvent->setDeviceTimestamp(static_cast<double>(timestamp)); 
            db->getDynamicInfo().addEvent(memEvent);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, memEvent);
          } else if(trace.EventType == XCL_PERF_MON_END_EVENT) {
            // may have to log start and end both
            // technically matching start can have ID 0, but for now assume matchingStart=0 means no start found. So log both start and end
            VTFEvent* matchingStart = db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID);
            if(nullptr == matchingStart || trace.Reserved == 1) {
              // add dummy start event
              memEvent = new KernelMemoryAccess(0, hostTimestamp, KERNEL_READ, deviceId);
              memEvent->setDeviceTimestamp(static_cast<double>(timestamp)); 
              db->getDynamicInfo().addEvent(memEvent);
              db->getDynamicInfo().markDeviceEventStart(trace.TraceID, memEvent);
              matchingStart = memEvent;
            }
            // add end event
            memEvent = new KernelMemoryAccess(matchingStart->getEventId(), hostTimestamp, KERNEL_READ, deviceId);
            memEvent->setDeviceTimestamp(static_cast<double>(timestamp)); 
            db->getDynamicInfo().addEvent(memEvent);
//            memEvent->setBurstLength(timestamp - ((KernelMemoryAccess*)matchingStart)->getDeviceTimestamp() + 1);
          }
        } else if(trace.TraceID & 1) {
          // KERNEL_WRITE
          s = trace.TraceID/2;
        Monitor* mon = db->getStaticInfo().getAIMonitor(deviceId, s);   
(void)mon; 
          if(trace.EventType == XCL_PERF_MON_START_EVENT) {
            memEvent = new KernelMemoryAccess(0, hostTimestamp, KERNEL_WRITE, deviceId);
            memEvent->setDeviceTimestamp(static_cast<double>(timestamp)); 
            db->getDynamicInfo().addEvent(memEvent);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, memEvent);
          } else if(trace.EventType == XCL_PERF_MON_END_EVENT) {
            // may have to log start and end both
            // technically matching start can have ID 0, but for now assume matchingStart=0 means no start found. So log both start and end
            VTFEvent* matchingStart = db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID);
            if(nullptr == matchingStart || trace.Reserved == 1) {
              // add dummy start event
              memEvent = new KernelMemoryAccess(0, hostTimestamp, KERNEL_WRITE, deviceId);
              memEvent->setDeviceTimestamp(static_cast<double>(timestamp)); 
              db->getDynamicInfo().addEvent(memEvent);
              db->getDynamicInfo().markDeviceEventStart(trace.TraceID, memEvent);
              matchingStart = memEvent;
            }
            // add end event
            memEvent = new KernelMemoryAccess(matchingStart->getEventId(), hostTimestamp, KERNEL_WRITE, deviceId);
            memEvent->setDeviceTimestamp(static_cast<double>(timestamp)); 
            db->getDynamicInfo().addEvent(memEvent);
//            memEvent->setBurstLength(timestamp - ((KernelMemoryAccess*)matchingStart)->getDeviceTimestamp() + 1);
          }
        }
      } // SPMPacket
      else if(SSPMPacket) {
//        Monitor* mon = db->getStaticInfo().getASMonitor(deviceId, s);   
//(void)mon;
        s = trace.TraceID - MIN_TRACE_ID_ASM;
 
      } // SSPMPacket
      else {}
    }

// LAST TRANSACTION ??
  }

#if 0
  void VPDynamicDatabase::markDeviceEventStart(uint64_t traceID, VTFEvent* event)
  {
    std::lock_guard<std::mutex> lock(dbLock);
    deviceEventStartMap[traceID].push_back(event) ;
  }

  VTFEvent* VPDynamicDatabase::matchingDeviceEventStart(uint64_t traceID)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    if (deviceEventStartMap.find(traceID) != deviceEventStartMap.end() && !deviceEventStartMap[traceID].empty())
    {
      VTFEvent* startEvent = deviceEventStartMap[traceID].front();
      deviceEventStartMap[traceID].pop_front();
      return startEvent ;
    }
    return nullptr;
  }

  void VPDynamicDatabase::markStart(uint64_t functionID, uint64_t eventID)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    startMap[functionID] = eventID ;
  }

  uint64_t VPDynamicDatabase::matchingStart(uint64_t functionID)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    if (startMap.find(functionID) != startMap.end())
    {
      uint64_t value = startMap[functionID] ;
      startMap.erase(functionID) ;
      return value ;
    }
    return 0 ;
  }
#endif

  // Complete training to convert device timestamp to host time domain
  // NOTE: see description of PTP @ http://en.wikipedia.org/wiki/Precision_Time_Protocol
  // clock training relation is linear within small durations (1 sec)
  // x, y coordinates are used for clock training
  void DeviceEventCreatorFromTrace::trainDeviceHostTimestamps(uint64_t deviceTimestamp, uint64_t hostTimestamp)
  {
    static double y1 = 0.0;
    static double y2 = 0.0;
    static double x1 = 0.0;
    static double x2 = 0.0;
//    bool isDeviceFlow = mPluginHandle->getFlowMode() == xdp::RTUtil::DEVICE;
    if (!y1 && !x1) {
      y1 = static_cast <double> (hostTimestamp);
      x1 = static_cast <double> (deviceTimestamp);
    } else {
      y2 = static_cast <double> (hostTimestamp);
      x2 = static_cast <double> (deviceTimestamp);
      // slope in ns/cycle
//      if (isDeviceFlow) {
        mTrainSlope = 1000.0/mTraceClockRateMHz;
//      } else {
//        mTrainSlope[type] = (y2 - y1) / (x2 - x1);
//      }
      mTrainOffset = y2 - mTrainSlope * x2;
      // next time update x1, y1
      y1 = 0.0;
      x1 = 0.0;
    }
  }

  // Convert device timestamp to host time domain (in msec)
  double DeviceEventCreatorFromTrace::convertDeviceToHostTimestamp(uint64_t deviceTimestamp)
  {
    // Return y = m*x + b
    return (mTrainSlope * (double)deviceTimestamp + mTrainOffset)/1e6;
  }


}
