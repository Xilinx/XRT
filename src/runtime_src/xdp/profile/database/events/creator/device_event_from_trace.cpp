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

  DeviceEventCreatorFromTrace::DeviceEventCreatorFromTrace(uint64_t devId)
    : deviceId(devId),
      db(VPDatabase::Instance()),
      clockTrainOffset(0),
      traceClockRateMHz(0),
      clockTrainSlope(0) 
  {
    traceClockRateMHz = db->getStaticInfo().getClockRateMHz(deviceId);
    clockTrainSlope = 1000.0/traceClockRateMHz;

    traceIDs.resize(db->getStaticInfo().getNumAM(deviceId));
    cuStarts.resize(db->getStaticInfo().getNumAM(deviceId));
    amLastTrans.resize(db->getStaticInfo().getNumAM(deviceId));

    aimLastTrans.resize(db->getStaticInfo().getNumAIM(deviceId));
    asmLastTrans.resize(db->getStaticInfo().getNumASM(deviceId));
  }

  void DeviceEventCreatorFromTrace::createDeviceEvents(xclTraceResultsVector& traceVector)
  {
    // Create Device Events and log them : do what is done in TraceParser::logTrace
    if(traceVector.mLength == 0)
      return;

    if(!VPDatabase::alive()) {
      return;
    }
    uint64_t timestamp = 0;
    for(unsigned int i=0; i < traceVector.mLength; i++) {
      auto& trace = traceVector.mArray[i];
      
      timestamp = trace.Timestamp;

      // assign EVENT_TYPE

      if (trace.isClockTrain) {
        trainDeviceHostTimestamps(timestamp, trace.HostTimestamp);
      }

      uint32_t s = 0;
      bool AMPacket  = (trace.TraceID >= MIN_TRACE_ID_AM && trace.TraceID <= MAX_TRACE_ID_AM);
      bool AIMPacket = (trace.TraceID >= MIN_TRACE_ID_AIM && trace.TraceID <= MAX_TRACE_ID_AIM);
      bool ASMPacket = (trace.TraceID >= MIN_TRACE_ID_ASM && trace.TraceID < MAX_TRACE_ID_ASM);
      if (!AMPacket && !AIMPacket && !ASMPacket) {
        continue;
      }
      double hostTimestamp = convertDeviceToHostTimestamp(timestamp);
      if (AMPacket) {
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
            if(cuStarts[s].empty()) {
              continue;
            }
            cuStarts[s].pop_front();
            event = new KernelDeviceEvent(e->getEventId(), hostTimestamp, deviceId, cuId);
            event->setDeviceTimestamp(timestamp);
            db->getDynamicInfo().addEvent(event);
          } else {
            // start event
            event = new KernelDeviceEvent(0, hostTimestamp, deviceId, cuId);
            event->setDeviceTimestamp(timestamp);
            db->getDynamicInfo().addEvent(event);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, event);
            cuStarts[s].push_back(event);
          }
        }
 
        if(stallIntEvent) {
          KernelStall* event = nullptr;
          if(traceIDs[s] & XAM_TRACE_STALL_INT_MASK) {
            // end event
            event = new KernelStall(db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID)->getEventId(),
                             hostTimestamp, KERNEL_STALL_DATAFLOW, deviceId);
            event->setDeviceTimestamp(timestamp);
            db->getDynamicInfo().addEvent(event);
          } else {
            // start event
            event = new KernelStall(0, hostTimestamp, KERNEL_STALL_DATAFLOW, deviceId);
            event->setDeviceTimestamp(timestamp);
            db->getDynamicInfo().addEvent(event);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, event);
          }
        } 

        if(stallStrEvent) {
          KernelStall* event = nullptr;
          if(traceIDs[s] & XAM_TRACE_STALL_STR_MASK) {
            // end event
            event = new KernelStall(db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID)->getEventId(),
                             hostTimestamp, KERNEL_STALL_PIPE, deviceId);
            event->setDeviceTimestamp(timestamp);
            db->getDynamicInfo().addEvent(event);
          } else {
            // start event
            event = new KernelStall(0, hostTimestamp, KERNEL_STALL_PIPE, deviceId);
            event->setDeviceTimestamp(timestamp);
            db->getDynamicInfo().addEvent(event);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, event);
          }
        } 
        if(stallExtEvent) {
          KernelStall* event = nullptr;
          if(traceIDs[s] & XAM_TRACE_STALL_EXT_MASK) {
            // end event
            event = new KernelStall(db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID)->getEventId(),
                             hostTimestamp, KERNEL_STALL_EXT_MEM, deviceId);
            event->setDeviceTimestamp(timestamp);
            db->getDynamicInfo().addEvent(event);
          } else {
            // start event
            event = new KernelStall(0, hostTimestamp, KERNEL_STALL_EXT_MEM, deviceId);
            event->setDeviceTimestamp(timestamp);
            db->getDynamicInfo().addEvent(event);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, event);
          }
        }
        traceIDs[s] ^= (trace.TraceID & 0xf);
        amLastTrans[s] = timestamp;
      } // AMPacket
      else if(AIMPacket) {
        KernelMemoryAccess* memEvent = nullptr;
        if((!trace.TraceID) & 1) { // read packet
          s = trace.TraceID/2;
        Monitor* mon = db->getStaticInfo().getAIMonitor(deviceId, s);  
(void)mon; 
          // KERNEL_READ
          if(trace.EventType == XCL_PERF_MON_START_EVENT) {
            memEvent = new KernelMemoryAccess(0, hostTimestamp, KERNEL_READ, deviceId);
            memEvent->setDeviceTimestamp(timestamp); 
            db->getDynamicInfo().addEvent(memEvent);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, memEvent);
          } else if(trace.EventType == XCL_PERF_MON_END_EVENT) {
            // may have to log start and end both
            // technically matching start can have ID 0, but for now assume matchingStart=0 means no start found. So log both start and end
            VTFEvent* matchingStart = db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID);
            if(nullptr == matchingStart || trace.Reserved == 1) {
              // add dummy start event
              memEvent = new KernelMemoryAccess(0, hostTimestamp, KERNEL_READ, deviceId);
              memEvent->setDeviceTimestamp(timestamp); 
              db->getDynamicInfo().addEvent(memEvent);
              db->getDynamicInfo().markDeviceEventStart(trace.TraceID, memEvent);
              matchingStart = memEvent;
            }
            // add end event
            memEvent = new KernelMemoryAccess(matchingStart->getEventId(), hostTimestamp, KERNEL_READ, deviceId);
            memEvent->setDeviceTimestamp(timestamp); 
            db->getDynamicInfo().addEvent(memEvent);
            aimLastTrans[s] = timestamp;
//            memEvent->setBurstLength(timestamp - ((KernelMemoryAccess*)matchingStart)->getDeviceTimestamp() + 1);
          }
        } else if(trace.TraceID & 1) {
          // KERNEL_WRITE
          s = trace.TraceID/2;
        Monitor* mon = db->getStaticInfo().getAIMonitor(deviceId, s);   
(void)mon; 
          if(trace.EventType == XCL_PERF_MON_START_EVENT) {
            memEvent = new KernelMemoryAccess(0, hostTimestamp, KERNEL_WRITE, deviceId);
            memEvent->setDeviceTimestamp(timestamp); 
            db->getDynamicInfo().addEvent(memEvent);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, memEvent);
          } else if(trace.EventType == XCL_PERF_MON_END_EVENT) {
            // may have to log start and end both
            VTFEvent* matchingStart = db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID);
            if(nullptr == matchingStart || trace.Reserved == 1) {
              // add dummy start event
              memEvent = new KernelMemoryAccess(0, hostTimestamp, KERNEL_WRITE, deviceId);
              memEvent->setDeviceTimestamp(timestamp); 
              db->getDynamicInfo().addEvent(memEvent);
              db->getDynamicInfo().markDeviceEventStart(trace.TraceID, memEvent);
              matchingStart = memEvent;
            }
            // add end event
            memEvent = new KernelMemoryAccess(matchingStart->getEventId(), hostTimestamp, KERNEL_WRITE, deviceId);
            memEvent->setDeviceTimestamp(timestamp); 
            db->getDynamicInfo().addEvent(memEvent);
            aimLastTrans[s] = timestamp;
//            memEvent->setBurstLength(timestamp - ((KernelMemoryAccess*)matchingStart)->getDeviceTimestamp() + 1);
          }
        }
      } // AIMPacket
      else if(ASMPacket) {
        s = trace.TraceID - MIN_TRACE_ID_ASM;

        Monitor* mon = db->getStaticInfo().getASMonitor(deviceId, s);

        bool isSingle =    trace.EventFlags & 0x10;
        bool txEvent =     trace.EventFlags & 0x8;
        bool stallEvent =  trace.EventFlags & 0x4;
        bool starveEvent = trace.EventFlags & 0x2;
        bool isStart =     trace.EventFlags & 0x1;

        VTFEventType streamEventType = KERNEL_STREAM_WRITE;
        if(txEvent) {
          streamEventType = (mon->isRead) ? KERNEL_STREAM_READ : KERNEL_STREAM_WRITE;
        } else if(starveEvent) {
          streamEventType = (mon->isRead) ? KERNEL_STREAM_READ_STARVE : KERNEL_STREAM_WRITE_STARVE;
        } else if(stallEvent) {
          streamEventType = (mon->isRead) ? KERNEL_STREAM_READ_STALL : KERNEL_STREAM_WRITE_STALL;
        }

        KernelStreamAccess* strmEvent = nullptr;
        if(isStart) {
          // start event
          strmEvent = new KernelStreamAccess(0, hostTimestamp, streamEventType, deviceId);
          strmEvent->setDeviceTimestamp(timestamp);
          db->getDynamicInfo().addEvent(strmEvent);
          db->getDynamicInfo().markDeviceEventStart(trace.TraceID, strmEvent);
        } else {
          VTFEvent* matchingStart = db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID);
          if(isSingle || nullptr == matchingStart) {
            // add dummy start event
            strmEvent = new KernelStreamAccess(0, hostTimestamp, streamEventType, deviceId);
            strmEvent->setDeviceTimestamp(timestamp); 
            db->getDynamicInfo().addEvent(strmEvent);
            db->getDynamicInfo().markDeviceEventStart(trace.TraceID, strmEvent);
            matchingStart = strmEvent;
          }
          // add end event
          strmEvent = new KernelStreamAccess(matchingStart->getEventId(), hostTimestamp, streamEventType, deviceId);
          strmEvent->setDeviceTimestamp(timestamp); 
          db->getDynamicInfo().addEvent(strmEvent);
          asmLastTrans[s] = timestamp;
        }
      } // ASMPacket
      else {}
    }
  }

  void DeviceEventCreatorFromTrace::end()
  {
    for(uint64_t amIndex = 0; amIndex < cuStarts.size(); ++amIndex) {
      if(cuStarts[amIndex].empty()) {
        continue;
      }
      // slot : amIndex
      // start event, end event
      // start end must have created already
      // check if the memory ports on current cu has any event

      VTFDeviceEvent* cuStartEvent = cuStarts[amIndex].front();
      uint64_t cuLastTimestamp  = amLastTrans[amIndex];

      // get CU Id for the current slot
      Monitor* am = db->getStaticInfo().getAMonitor(deviceId, amIndex);
      int32_t  cuId = am->cuIndex;

      // Check if any memory port on current CU had a trace packet
      for(uint64_t aimIndex = 0; aimIndex < aimLastTrans.size(); ++aimIndex) {
        /* To reduce overhead, first check the timestamp.
         * If last activity timestamp on CU is earlier than current AIM, then only check
         * whether the current AIM is attached to the same CU.
         */
        if(cuLastTimestamp >= aimLastTrans[aimIndex]) {
          continue;
        }
        Monitor* aim = db->getStaticInfo().getAIMonitor(deviceId, aimIndex);
        if(cuId != aim->cuIndex) {
          // current AIM attached to a different CU, so continue
          continue;
        }
        // Update lastTimestamp as last activity on the AIM for the current CU is later
        // than what was recorded for the CU(AM) itself.
        cuLastTimestamp = aimLastTrans[aimIndex];
      }
      // Check if any streaming port on current CU had a trace packet
      for(uint64_t asmIndex = 0; asmIndex < asmLastTrans.size(); ++asmIndex) {
        /* To reduce overhead, first check the timestamp.
         * If last activity timestamp on CU is earlier than current ASM, then only check
         * whether the current ASM is attached to the same CU.
         */
        if(cuLastTimestamp >= asmLastTrans[asmIndex]) {
          continue;
        }
        Monitor* asM = db->getStaticInfo().getASMonitor(deviceId, asmIndex);
        if(cuId != asM->cuIndex) {
          // current ASM attached to a different CU, so continue
          continue;
        }
        // Update lastTimestamp as last activity on the ASM for the current CU is later
        // than what was recorded for the CU(AM) itself.
        cuLastTimestamp = asmLastTrans[asmIndex];
      }
      if(0 == cuLastTimestamp) {
        continue; // nothing to do? what about unmatched start?
      }
      // Warning : "Incomplete CU profile trace detected. Timeline trace will have approximate CU End."

      // end event
      cuStarts[amIndex].pop_front();
      
      double hostTimestamp = convertDeviceToHostTimestamp(cuLastTimestamp);
      KernelDeviceEvent* event = new KernelDeviceEvent(cuStartEvent->getEventId(), hostTimestamp, deviceId, cuId);
      event->setDeviceTimestamp(cuLastTimestamp);
      db->getDynamicInfo().addEvent(event); 
    }
  }

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
    if (!y1 && !x1) {
      y1 = static_cast <double> (hostTimestamp);
      x1 = static_cast <double> (deviceTimestamp);
    } else {
      y2 = static_cast <double> (hostTimestamp);
      x2 = static_cast <double> (deviceTimestamp);
      // slope in ns/cycle
//      if (isDeviceFlow) {
        clockTrainSlope = 1000.0/traceClockRateMHz;
//      } else {
//        clockTrainSlope = (y2 - y1) / (x2 - x1);
//      }
      clockTrainOffset = y2 - clockTrainSlope * x2;
      // next time update x1, y1
      y1 = 0.0;
      x1 = 0.0;
    }
  }

  // Convert device timestamp to host time domain (in msec)
  double DeviceEventCreatorFromTrace::convertDeviceToHostTimestamp(uint64_t deviceTimestamp)
  {
    return ((clockTrainSlope * (double)deviceTimestamp) + clockTrainOffset)/1e6;
  }



}
