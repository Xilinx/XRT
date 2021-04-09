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
#include "xdp/profile/plugin/vp_base/utility.h"

#include "core/common/message.h"

#ifdef _WIN32
#pragma warning (disable : 4244)
/* Disable warnings for conversion from uint32_t to uint16_t */
#endif

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

    xclbin = (db->getStaticInfo()).getCurrentlyLoadedXclbin(devId) ;

    auto numAM = (db->getStaticInfo()).getNumAM(deviceId, xclbin) ;
    traceIDs.resize(numAM);
    cuStarts.resize(numAM);
    amLastTrans.resize(numAM);

    aimLastTrans.resize((db->getStaticInfo()).getNumAIM(deviceId, xclbin)) ;
    asmLastTrans.resize((db->getStaticInfo()).getNumASM(deviceId, xclbin)) ;
  }

  void DeviceEventCreatorFromTrace::addCUEvent(xclTraceResults& trace,
                                               double hostTimestamp,
                                               uint32_t s,
                                               uint64_t monTraceID,
                                               int32_t cuId)
  {
    KernelEvent* event = nullptr;

    if (!(trace.EventFlags & XAM_TRACE_CU_MASK)) {
      // End event
      VTFEvent* e =
        db->getDynamicInfo().matchingDeviceEventStart(monTraceID, KERNEL);
      if(!e) {
        return ;
      }
      if (cuStarts[s].empty()) {
        return ;
      }

      // Execution time = (end time) - (start time)
      auto startTimeEvent = cuStarts[s].front();
      auto startTime = convertDeviceToHostTimestamp(startTimeEvent->getDeviceTimestamp());
      auto executionTime = hostTimestamp - startTime;

      cuStarts[s].pop_front();
      event = new KernelEvent(e->getEventId(), hostTimestamp, KERNEL, deviceId, s, cuId);
      event->setDeviceTimestamp(trace.Timestamp);
      db->getDynamicInfo().addEvent(event);
      (db->getStats()).setLastKernelEndTime(hostTimestamp) ;

      // Log a CU execution in our statistics database
      // NOTE: At this stage, we don't know the global work size, so let's
      //       leave it to the database to fill that in.
      auto cu = db->getStaticInfo().getCU(deviceId, cuId);
      (db->getStats()).logComputeUnitExecution(cu->getName(),
                                               cu->getKernelName(),
                                               cu->getDim(),
                                               "",
                                               executionTime);
    }
    else {
      // start event
      event = new KernelEvent(0, hostTimestamp, KERNEL, deviceId, s, cuId);
      event->setDeviceTimestamp(trace.Timestamp);
      db->getDynamicInfo().addEvent(event);
      db->getDynamicInfo().markDeviceEventStart(monTraceID, event);

      cuStarts[s].push_back(event);
      if(1 == cuStarts[s].size()) {
        traceIDs[s] = 0; // When current CU starts, reset stall status
      }
      if (db->getStats().getFirstKernelStartTime() == 0.0)
        (db->getStats()).setFirstKernelStartTime(hostTimestamp) ;
      }
  }

  void DeviceEventCreatorFromTrace::addStallEvent(xclTraceResults& trace,
                                                  double hostTimestamp,
                                                  uint32_t s,
                                                  uint64_t monTraceID,
                                                  int32_t cuId,
                                                  VTFEventType type,
                                                  uint64_t mask)
  {
    KernelStall* event = nullptr;
    if (traceIDs[s] & mask) {
      // End event
      VTFEvent* startEvent =
        db->getDynamicInfo().matchingDeviceEventStart(monTraceID, type) ;
      event = new KernelStall(startEvent->getEventId(),
                              hostTimestamp,
                              type,
                              deviceId,
                              s,
                              cuId) ;
      event->setDeviceTimestamp(trace.Timestamp);
      db->getDynamicInfo().addEvent(event);
    }
    else {
      // Start event
      event = new KernelStall(0, hostTimestamp, type, deviceId, s, cuId);
      event->setDeviceTimestamp(trace.Timestamp);
      db->getDynamicInfo().addEvent(event);
      db->getDynamicInfo().markDeviceEventStart(monTraceID, event);
    }
  }

  void DeviceEventCreatorFromTrace::addAMEvent(xclTraceResults& trace,
                                               double hostTimestamp)
  {
    uint32_t s = ((trace.TraceID - MIN_TRACE_ID_AM) / 16);
    uint64_t monTraceID = s*16 + MIN_TRACE_ID_AM;

    Monitor* mon  = db->getStaticInfo().getAMonitor(deviceId, xclbin, s);
    if (!mon) {
      // In hardware emulation, there might be monitors inserted
      //  that don't show up in the debug ip layout.  These are added
      //  for their own debugging purposes and we should ignore any
      //  packets we see from them.
      return ;
    }

    int32_t cuId = mon->cuIndex;

    if (trace.TraceID & XAM_TRACE_CU_MASK) {
      addCUEvent(trace, hostTimestamp, s, monTraceID, cuId);
    }
    if (trace.TraceID & XAM_TRACE_STALL_INT_MASK) {
      addStallEvent(trace, hostTimestamp, s, monTraceID, cuId,
                    KERNEL_STALL_DATAFLOW, XAM_TRACE_STALL_INT_MASK);
    }
    if (trace.TraceID & XAM_TRACE_STALL_STR_MASK) {
      addStallEvent(trace, hostTimestamp, s, monTraceID, cuId,
                    KERNEL_STALL_PIPE, XAM_TRACE_STALL_STR_MASK);
    }
    if (trace.TraceID & XAM_TRACE_STALL_EXT_MASK) {
      addStallEvent(trace, hostTimestamp, s, monTraceID, cuId,
                    KERNEL_STALL_EXT_MEM, XAM_TRACE_STALL_EXT_MASK);
    }

    traceIDs[s] ^= (trace.TraceID & 0xf);
    amLastTrans[s] = trace.Timestamp;

    // If a CU just ended completely, we need to tie off any hanging
    //  reads, writes, and stalls
    if ((trace.TraceID & XAM_TRACE_CU_MASK) && cuStarts[s].empty()) {
      addApproximateDataTransferEndEvents(cuId);
      addApproximateStallEndEvents(trace, hostTimestamp, s, monTraceID, cuId);
    }
  }

  void DeviceEventCreatorFromTrace::addAIMEvent(xclTraceResults& trace,
                                                double hostTimestamp)
  {
    uint32_t slot = trace.TraceID / 2 ;
    Monitor* mon = db->getStaticInfo().getAIMonitor(deviceId, xclbin, slot);
    if (!mon) {
      // In hardware emulation, there might be monitors inserted that
      //  don't show up in the debug ip layout.  These are added for
      //  their own debugging purposes and we should ignore any packets
      //  we see from them
      return ;
    }
    int32_t cuId = mon->cuIndex ;
    VTFEventType ty = (trace.TraceID & 1) ? KERNEL_WRITE : KERNEL_READ ;

    addKernelDataTransferEvent(ty, trace, slot, cuId, hostTimestamp);
  }

  void
  DeviceEventCreatorFromTrace::
  addKernelDataTransferEvent(VTFEventType ty,
                             xclTraceResults& trace,
                             uint32_t slot,
                             int32_t cuId,
                             double hostTimestamp)
  {
    DeviceMemoryAccess* memEvent = nullptr;
    double halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;

    if (trace.EventType == XCL_PERF_MON_START_EVENT) {
      // If we see two starts in a row of the same type on the same slot,
      //  then we must have dropped an end packet.  Add a dummy end packet
      //  here.
      if (db->getDynamicInfo().hasMatchingDeviceEventStart(trace.TraceID, ty)){
        VTFEvent* matchingStart =
          db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID, ty);
        memEvent =
          new DeviceMemoryAccess(matchingStart->getEventId(),
                                 hostTimestamp - halfCycleTimeInMs,
                                 ty, deviceId, slot, cuId);
        memEvent->setDeviceTimestamp(trace.Timestamp);
        db->getDynamicInfo().addEvent(memEvent);
        aimLastTrans[slot] = trace.Timestamp;
      }

      memEvent = new DeviceMemoryAccess(0, hostTimestamp, ty, deviceId, slot, cuId) ;
      memEvent->setDeviceTimestamp(trace.Timestamp) ;
      db->getDynamicInfo().addEvent(memEvent) ;
      db->getDynamicInfo().markDeviceEventStart(trace.TraceID, memEvent);
    } else if (trace.EventType == XCL_PERF_MON_END_EVENT) {
      VTFEvent* matchingStart = db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID, ty);
      if (nullptr == matchingStart) {
        // We need to add a dummy start event for this observed end event
        memEvent = new DeviceMemoryAccess(0, hostTimestamp, ty, deviceId, slot, cuId);
        memEvent->setDeviceTimestamp(trace.Timestamp);
        db->getDynamicInfo().addEvent(memEvent);
        matchingStart = memEvent;

        // Also, progress time so the end is after the start
        hostTimestamp += halfCycleTimeInMs;
      } else if (trace.Reserved == 1) {
        // We have a matching start, so we need to end it
        if (matchingStart->getTimestamp() == hostTimestamp) {
          // All we have to do is push time forward and let this end event
          //  match the start we found
          hostTimestamp += halfCycleTimeInMs;
        } else {
          // The times are different, so we need to end the matching start
          //  and then create an additional pulse
          memEvent = new DeviceMemoryAccess(matchingStart->getEventId(), hostTimestamp, ty, deviceId, slot, cuId);
          memEvent->setDeviceTimestamp(trace.Timestamp) ;
          db->getDynamicInfo().addEvent(memEvent);

          // Now create the dummy start
          memEvent = new DeviceMemoryAccess(0, hostTimestamp, ty, deviceId, slot, cuId);
          memEvent->setDeviceTimestamp(trace.Timestamp);
          db->getDynamicInfo().addEvent(memEvent);
          matchingStart = memEvent;
          // Also, progress time so the end is after the start
          hostTimestamp += halfCycleTimeInMs;
        }
      }

      // The true end event we observed
      memEvent = new DeviceMemoryAccess(matchingStart->getEventId(), hostTimestamp, ty, deviceId, slot, cuId);
      memEvent->setDeviceTimestamp(trace.Timestamp);
      db->getDynamicInfo().addEvent(memEvent);
      aimLastTrans[slot] = trace.Timestamp;
    }
  }

  void DeviceEventCreatorFromTrace::createDeviceEvents(std::vector<xclTraceResults>& traceVector)
  {
    // Create Device Events and log them : do what is done in TraceParser::logTrace
    if(traceVector.size() == 0)
      return;

    if(!VPDatabase::alive()) {
      return;
    }

    uint64_t timestamp = 0;
    double halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;

    for (auto& trace : traceVector) {
      timestamp = trace.Timestamp;

      if (trace.isClockTrain) {
        trainDeviceHostTimestamps(timestamp, trace.HostTimestamp);
        continue;
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
        addAMEvent(trace, hostTimestamp);
      }
      else if(AIMPacket) {
        addAIMEvent(trace, hostTimestamp) ;
      }
      else if(ASMPacket) {
        s = trace.TraceID - MIN_TRACE_ID_ASM;

        Monitor* mon  = db->getStaticInfo().getASMonitor(deviceId, xclbin, s);
        if (!mon) {
          // In hardware emulation, there might be monitors inserted
          //  that don't show up in the debug ip layout.  These are added
          //  for their own debugging purposes and we should ignore any
          //  packets we see from them.
          continue ;
        }
        int32_t  cuId = mon->cuIndex;

        bool isSingle    = trace.EventFlags & 0x10;
        bool txEvent     = trace.EventFlags & 0x8;
        bool stallEvent  = trace.EventFlags & 0x4;
        bool starveEvent = trace.EventFlags & 0x2;
        bool isStart     = trace.EventFlags & 0x1;

        VTFEventType streamEventType = KERNEL_STREAM_WRITE;
        if(txEvent) {
          streamEventType = (mon->isRead) ? KERNEL_STREAM_READ : KERNEL_STREAM_WRITE;
        } else if(starveEvent) {
          streamEventType = (mon->isRead) ? KERNEL_STREAM_READ_STARVE : KERNEL_STREAM_WRITE_STARVE;
        } else if(stallEvent) {
          streamEventType = (mon->isRead) ? KERNEL_STREAM_READ_STALL : KERNEL_STREAM_WRITE_STALL;
        }

        DeviceStreamAccess* strmEvent = nullptr;
        if(isStart) {
          // start event
          strmEvent = new DeviceStreamAccess(0, hostTimestamp, streamEventType, deviceId, s, cuId);
          strmEvent->setDeviceTimestamp(timestamp);
          db->getDynamicInfo().addEvent(strmEvent);
          db->getDynamicInfo().markDeviceEventStart(trace.TraceID, strmEvent);
        } else {
          VTFEvent* matchingStart = db->getDynamicInfo().matchingDeviceEventStart(trace.TraceID, streamEventType);
          if(isSingle || nullptr == matchingStart) {
            // add dummy start event
            strmEvent = new DeviceStreamAccess(0, hostTimestamp, streamEventType, deviceId, s, cuId);
            strmEvent->setDeviceTimestamp(timestamp); 
            db->getDynamicInfo().addEvent(strmEvent);
            matchingStart = strmEvent;
            hostTimestamp += halfCycleTimeInMs;
          }
          // add end event
          strmEvent = new DeviceStreamAccess(matchingStart->getEventId(), hostTimestamp, streamEventType, deviceId, s, cuId);
          strmEvent->setDeviceTimestamp(timestamp); 
          db->getDynamicInfo().addEvent(strmEvent);
          asmLastTrans[s] = timestamp;
        }
      } // ASMPacket
      else {}
    }
  }

  void DeviceEventCreatorFromTrace::addApproximateCUEndEvents()
  {
    for(uint32_t amIndex = 0; amIndex < cuStarts.size(); ++amIndex) {
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
      Monitor* am = db->getStaticInfo().getAMonitor(deviceId, xclbin, amIndex);
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
        Monitor* aim = db->getStaticInfo().getAIMonitor(deviceId, xclbin, aimIndex);
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
        Monitor* asM = db->getStaticInfo().getASMonitor(deviceId, xclbin, asmIndex);
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
      const char* msg = "Incomplete CU profile trace detected. Timeline trace will have approximate CU End.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg) ;

      // end event
      cuStarts[amIndex].pop_front();
      
      double hostTimestamp = convertDeviceToHostTimestamp(cuLastTimestamp);
      KernelEvent* event = new KernelEvent(cuStartEvent->getEventId(), hostTimestamp, KERNEL, deviceId, amIndex, cuId);
      event->setDeviceTimestamp(cuLastTimestamp);
      db->getDynamicInfo().addEvent(event); 
    }
  }

  void DeviceEventCreatorFromTrace::addApproximateDataTransferEvent(VTFEventType type, uint64_t aimTraceID, int32_t amId, int32_t cuId)
  {
    VTFEvent* startEvent =
      db->getDynamicInfo().matchingDeviceEventStart(aimTraceID, type);
    if (!startEvent)
      return ;

    uint64_t transStartTimestamp = 0 ;
    uint64_t transApproxEndTimestamp = 0 ;
    double transApproxEndHostTimestamp = 0 ;

    VTFDeviceEvent* deviceStartEvent=dynamic_cast<VTFDeviceEvent*>(startEvent);
    if (!deviceStartEvent)
      return ;

    const double halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;

    transStartTimestamp = deviceStartEvent->getDeviceTimestamp();
    if (amId == -1) {
      // This is a floating AIM monitor not attached to any particular CU.
      transApproxEndTimestamp = transStartTimestamp ;
      transApproxEndHostTimestamp = convertDeviceToHostTimestamp(transStartTimestamp) + halfCycleTimeInMs ;
    }
    else {
      // Check the last known transaction on the CU to approximate the end
      uint64_t cuLastTimestamp  = amLastTrans[amId];
      if (transStartTimestamp < cuLastTimestamp) {
        transApproxEndTimestamp = cuLastTimestamp ;
        transApproxEndHostTimestamp = convertDeviceToHostTimestamp(cuLastTimestamp);
      }
      else {
        transApproxEndTimestamp = transStartTimestamp ;
        transApproxEndHostTimestamp = convertDeviceToHostTimestamp(transStartTimestamp) + halfCycleTimeInMs ;
      }
    }
    // Add approximate end event
    DeviceMemoryAccess* endEvent =
      new DeviceMemoryAccess(startEvent->getEventId(),
                             transApproxEndHostTimestamp,
                             type,
                             deviceId,
                             amId,
                             cuId);
    endEvent->setDeviceTimestamp(transApproxEndTimestamp);
    db->getDynamicInfo().addEvent(endEvent);
  }

  void DeviceEventCreatorFromTrace::addApproximateDataTransferEndEvents()
  {
    // Go through all of our AIMs.  If any of them have any outstanding
    //  reads or writes, then finish them based on the last CU execution time.
    for (uint64_t aimIndex = 0 ;
         aimIndex < (db->getStaticInfo()).getNumAIM(deviceId, xclbin) ;
         ++aimIndex) {

      uint64_t aimTraceID = aimIndex + MIN_TRACE_ID_AIM ;
      Monitor* mon =
        db->getStaticInfo().getAIMonitor(deviceId, xclbin, aimIndex);
      if (!mon)
        continue;

      int32_t cuId = mon->cuIndex ;
      int32_t amId = -1 ;

      if (cuId != -1) {
        ComputeUnitInstance* cu = db->getStaticInfo().getCU(deviceId, cuId);
        if (cu) {
          amId = cu->getAccelMon();
        }
      }

      addApproximateDataTransferEvent(KERNEL_READ, aimTraceID, amId, cuId) ;
      addApproximateDataTransferEvent(KERNEL_WRITE, aimTraceID, amId, cuId) ;
    }
  }

  void DeviceEventCreatorFromTrace::addApproximateDataTransferEndEvents(int32_t cuId)
  {
    if (cuId == -1)
      return ;
    for (uint64_t aimIndex = 0 ;
         aimIndex < (db->getStaticInfo()).getNumAIM(deviceId, xclbin) ;
         ++aimIndex) {

      uint64_t aimTraceID = aimIndex + MIN_TRACE_ID_AIM ;
      Monitor* mon =
        db->getStaticInfo().getAIMonitor(deviceId, xclbin, aimIndex);
      if (!mon)
        continue;

      if (cuId != mon->cuIndex)
        continue ;
      int32_t amId = -1 ;
      ComputeUnitInstance* cu = db->getStaticInfo().getCU(deviceId, cuId);
      if (cu) {
        amId = cu->getAccelMon();
      }
      addApproximateDataTransferEvent(KERNEL_READ, aimTraceID, amId, cuId) ;
      addApproximateDataTransferEvent(KERNEL_WRITE, aimTraceID, amId, cuId) ;
    }
  }

  void DeviceEventCreatorFromTrace::addApproximateStreamEndEvents()
  {
    // Find unfinished ASM events
    bool unfinishedASMevents = false;
    for(uint64_t asmIndex = 0; asmIndex < (db->getStaticInfo()).getNumASM(deviceId, xclbin); ++asmIndex) {
      uint64_t asmTraceID = asmIndex + MIN_TRACE_ID_ASM;
      Monitor* mon  = db->getStaticInfo().getASMonitor(deviceId, xclbin, asmIndex);
      if(!mon) {
        continue;
      }
      int32_t  cuId = mon->cuIndex;
      int32_t  amId = -1;
      uint64_t cuLastTimestamp = 0, asmAppxLastTransTimeStamp = 0;
      if(-1 != cuId) {
        ComputeUnitInstance* cu = db->getStaticInfo().getCU(deviceId, cuId);
        if(cu) {
          amId = cu->getAccelMon();
        }
        if(-1 != amId) {
          cuLastTimestamp  = amLastTrans[amId];
        }
      }

      VTFEventType streamEventType = (mon->isRead) ? KERNEL_STREAM_READ : KERNEL_STREAM_WRITE;
      addApproximateStreamEndEvent(asmIndex, asmTraceID, streamEventType, cuId, amId, cuLastTimestamp, asmAppxLastTransTimeStamp, unfinishedASMevents);

      streamEventType = (mon->isRead) ? KERNEL_STREAM_READ_STALL : KERNEL_STREAM_WRITE_STALL;
      addApproximateStreamEndEvent(asmIndex, asmTraceID, streamEventType, cuId, amId, cuLastTimestamp, asmAppxLastTransTimeStamp, unfinishedASMevents);

      streamEventType = (mon->isRead) ? KERNEL_STREAM_READ_STARVE : KERNEL_STREAM_WRITE_STARVE;
      addApproximateStreamEndEvent(asmIndex, asmTraceID, streamEventType, cuId, amId, cuLastTimestamp, asmAppxLastTransTimeStamp, unfinishedASMevents);

      asmLastTrans[asmIndex] = asmAppxLastTransTimeStamp;
    }

    if(unfinishedASMevents) {
      const char* msg = "Found unfinished events on Stream connections. Adding approximate ends for Stream Activity/Stall/Starve on timeline trace.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg) ;
    }
  }

  void DeviceEventCreatorFromTrace::addApproximateStallEndEvents(xclTraceResults& trace, double hostTimestamp, uint32_t s, uint64_t monTraceID, int32_t cuId)
  {
    if (traceIDs[s] == 0)
      return ;
    // There are some stall events still outstanding that need to be closed
    const double halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;

    if (traceIDs[s] & XAM_TRACE_STALL_INT_MASK) {
      addStallEvent(trace, hostTimestamp-halfCycleTimeInMs, s, monTraceID, cuId,
                    KERNEL_STALL_DATAFLOW, XAM_TRACE_STALL_INT_MASK);
    }
    if (traceIDs[s] & XAM_TRACE_STALL_STR_MASK) {
      addStallEvent(trace, hostTimestamp-halfCycleTimeInMs, s, monTraceID,
                    cuId, KERNEL_STALL_PIPE, XAM_TRACE_STALL_STR_MASK);
    }
    if (traceIDs[s] & XAM_TRACE_STALL_EXT_MASK) {
      addStallEvent(trace, hostTimestamp-halfCycleTimeInMs, s, monTraceID, cuId,
                    KERNEL_STALL_EXT_MEM, XAM_TRACE_STALL_EXT_MASK);
    }
  }

  void DeviceEventCreatorFromTrace::end()
  {
    addApproximateCUEndEvents();
    addApproximateDataTransferEndEvents();
    addApproximateStreamEndEvents();
  }

  void DeviceEventCreatorFromTrace::addApproximateStreamEndEvent(uint64_t asmIndex, uint64_t asmTraceID, VTFEventType streamEventType, 
                                                                 int32_t cuId, int32_t  amId, uint64_t cuLastTimestamp,
                                                                 uint64_t &asmAppxLastTransTimeStamp, bool &unfinishedASMevents)
  {
    uint64_t asmStartTimestamp = 0, asmAppxEndTimestamp = 0;
    double   asmAppxEndHostTimestamp = 0;
    double   halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;

    VTFEvent* matchingStart = db->getDynamicInfo().matchingDeviceEventStart(asmTraceID, streamEventType);
    while(matchingStart) {
      unfinishedASMevents = true;
      asmStartTimestamp = (dynamic_cast<VTFDeviceEvent*>(matchingStart))->getDeviceTimestamp();
      if(-1 == amId) {
        // For floating ASM i.e. ASM not attached to any CU or for ASMs attached to free running CUs which don't have AM attached
        asmAppxEndTimestamp = asmStartTimestamp;
        asmAppxEndHostTimestamp = convertDeviceToHostTimestamp(asmStartTimestamp) + halfCycleTimeInMs;
      } else {
        asmAppxEndTimestamp = (asmStartTimestamp < cuLastTimestamp) ? cuLastTimestamp : asmStartTimestamp;
        asmAppxEndHostTimestamp = (asmStartTimestamp < cuLastTimestamp) ? convertDeviceToHostTimestamp(cuLastTimestamp) : (convertDeviceToHostTimestamp(asmStartTimestamp) + halfCycleTimeInMs);
      }
      asmAppxLastTransTimeStamp = (asmAppxLastTransTimeStamp < asmAppxEndTimestamp) ? asmAppxEndTimestamp : asmAppxLastTransTimeStamp;

      // Add approximate end event
      DeviceStreamAccess* strmEvent = new DeviceStreamAccess(matchingStart->getEventId(), asmAppxEndHostTimestamp,
                                                           streamEventType, deviceId, asmIndex, cuId);
      strmEvent->setDeviceTimestamp(asmAppxEndTimestamp);
      db->getDynamicInfo().addEvent(strmEvent);

      matchingStart = db->getDynamicInfo().matchingDeviceEventStart(asmTraceID, streamEventType);
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
      if (xdp::getFlowMode() == HW) {
        clockTrainSlope = 1000.0/traceClockRateMHz;
      } else {
        clockTrainSlope = (y2 - y1) / (x2 - x1);
      }
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
