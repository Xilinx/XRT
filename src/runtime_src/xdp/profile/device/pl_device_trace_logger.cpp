/**
 * Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_CORE_SOURCE

#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/pl_device_trace_logger.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/database/static_info/xclbin_info.h"

#include "core/common/message.h"
#include "xrt/experimental/xrt_profile.h"

#ifdef _WIN32
#pragma warning (disable : 4244)
/* Disable warnings for conversion from uint32_t to uint16_t */
#endif

namespace xdp {

  PLDeviceTraceLogger::PLDeviceTraceLogger(uint64_t devId)
    : deviceId(devId),
      db(VPDatabase::Instance()),
      clockTrainOffset(0),
      traceClockRateMHz(0),
      clockTrainSlope(0)
  {
    // This trace logger function is for PL only

    traceClockRateMHz = db->getStaticInfo().getPLMaxClockRateMHz(deviceId);
    clockTrainSlope = 1000.0/traceClockRateMHz;

    ConfigInfo* config = (db->getStaticInfo()).getCurrentlyLoadedConfig(devId);
    xclbin = config->getPlXclbin();
    if (!xclbin)
      return;

    // Use the total number of Accelerator Monitors for the size
    auto numAM = (db->getStaticInfo()).getNumAM(deviceId, xclbin);
    traceIDs.resize(numAM);
    cuStarts.resize(numAM);
    amLastTrans.resize(numAM);

    // Use the number of monitors in the PL region (not the shell) including
    //  any configured for just trace.
    aimLastTrans.resize((db->getStaticInfo()).getNumUserAIM(deviceId, xclbin));
    asmLastTrans.resize((db->getStaticInfo()).getNumUserASM(deviceId, xclbin));
  }

  void PLDeviceTraceLogger::addCUEndEvent(double hostTimestamp,
                                          uint64_t deviceTimestamp,
                                          uint32_t s,
                                          int32_t cuId)
  {
    // In addition to creating the event, we must log statistics

    // Execution time = (end time) - (start time)
    std::pair<uint64_t, uint64_t> startEventInfo = cuStarts[s].front();
    auto startEventID = startEventInfo.first;
    auto startTime = convertDeviceToHostTimestamp(startEventInfo.second);
    auto executionTime = hostTimestamp - startTime;

    cuStarts[s].pop_front();
    auto event = new KernelEvent(startEventID,
                                 hostTimestamp, KERNEL, deviceId, s, cuId);
    event->setDeviceTimestamp(deviceTimestamp);
    db->getDynamicInfo().addEvent(event);
    (db->getStats()).setLastKernelEndTime(hostTimestamp);

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

  void PLDeviceTraceLogger::addCUEvent(uint64_t trace,
                                       double hostTimestamp,
                                       uint32_t slot,
                                       uint64_t monTraceId,
                                       int32_t cuId)
  {
    KernelEvent* event = nullptr;
    uint64_t eventFlags = getEventFlags(trace);
    uint64_t deviceTimestamp = getDeviceTimestamp(trace);

    if (!(eventFlags & CU_MASK)) {
      // End event
      DeviceEventInfo e =
        db->getDynamicInfo().matchingDeviceEventStart(deviceId, monTraceId, KERNEL);
      if(e.type == UNKNOWN_EVENT)
        return;
      if (cuStarts[slot].empty())
        return;

      addCUEndEvent(hostTimestamp, deviceTimestamp, slot, cuId);
    }
    else {
      // start event
      event = new KernelEvent(0, hostTimestamp, KERNEL, deviceId, slot, cuId);
      event->setDeviceTimestamp(deviceTimestamp);
      db->getDynamicInfo().addEvent(event);
      DeviceEventInfo info;
      info.type = event->getEventType();
      info.eventID = event->getEventId();
      info.hostTimestamp = event->getTimestamp();
      info.deviceTimestamp = deviceTimestamp;
      db->getDynamicInfo().markDeviceEventStart(deviceId, monTraceId, info);

      cuStarts[slot].push_back(std::make_pair(event->getEventId(),
                                              deviceTimestamp));
      if(1 == cuStarts[slot].size()) {
        traceIDs[slot] = 0; // When current CU starts, reset stall status
      }
      if (db->getStats().getFirstKernelStartTime() == 0.0)
        (db->getStats()).setFirstKernelStartTime(hostTimestamp);
    }
  }

  void PLDeviceTraceLogger::addStallEvent(uint64_t trace,
                                          double hostTimestamp,
                                          uint32_t slot,
                                          uint64_t monTraceId,
                                          int32_t cuId,
                                          VTFEventType type,
                                          uint64_t mask)
  {
    uint64_t deviceTimestamp = getDeviceTimestamp(trace);

    KernelStall* event = nullptr;
    if (traceIDs[slot] & mask) {
      // End event
      DeviceEventInfo startEventInfo =
        db->getDynamicInfo().matchingDeviceEventStart(deviceId, monTraceId, type);
      event = new KernelStall(startEventInfo.eventID,
                              hostTimestamp,
                              type,
                              deviceId,
                              slot,
                              cuId);
      event->setDeviceTimestamp(deviceTimestamp);
      db->getDynamicInfo().addEvent(event);
    }
    else {
      // Start event
      event = new KernelStall(0, hostTimestamp, type, deviceId, slot, cuId);
      event->setDeviceTimestamp(deviceTimestamp);
      db->getDynamicInfo().addEvent(event);
      DeviceEventInfo info;
      info.type = event->getEventType();
      info.eventID = event->getEventId();
      info.hostTimestamp = event->getTimestamp();
      info.deviceTimestamp = deviceTimestamp;
      db->getDynamicInfo().markDeviceEventStart(deviceId, monTraceId, info);
    }
  }

  void PLDeviceTraceLogger::addAMEvent(uint64_t trace, double hostTimestamp)
  {
    uint64_t traceID = getTraceId(trace);
    uint64_t deviceTimestamp = getDeviceTimestamp(trace);

    uint32_t slot = (traceID - util::min_trace_id_am) / 16;
    uint64_t monTraceID = slot * 16 + util::min_trace_id_am;

    Monitor* mon = db->getStaticInfo().getAMonitor(deviceId, xclbin, slot);
    if (!mon) {
      // In hardware emulation, there might be monitors inserted
      //  that don't show up in the debug ip layout.  These are added
      //  for their own debugging purposes and we should ignore any
      //  packets we see from them.
      return ;
    }

    int32_t cuId = mon->cuIndex;

    // A single trace packet could have multiple events happening simultaneously

    if (traceID & CU_MASK) {
      addCUEvent(trace, hostTimestamp, slot, monTraceID, cuId);
    }
    if (traceID & STALL_INT_MASK) {
      addStallEvent(trace, hostTimestamp, slot, monTraceID, cuId,
                    KERNEL_STALL_DATAFLOW, STALL_INT_MASK);
    }
    if (traceID & STALL_STR_MASK) {
      addStallEvent(trace, hostTimestamp, slot, monTraceID, cuId,
                    KERNEL_STALL_PIPE, STALL_STR_MASK);
    }
    if (traceID & STALL_EXT_MASK) {
      addStallEvent(trace, hostTimestamp, slot, monTraceID, cuId,
                    KERNEL_STALL_EXT_MEM, STALL_EXT_MASK);
    }

    traceIDs[slot] ^= (traceID & 0xf);
    amLastTrans[slot] = deviceTimestamp;

    // If a CU just ended completely, we need to tie off any hanging
    //  reads, writes, and stalls
    if ((traceID & CU_MASK) && cuStarts[slot].empty()) {
      addApproximateDataTransferEndEvents(cuId);
      addApproximateStallEndEvents(trace, hostTimestamp, slot, monTraceID, cuId);
    }
  }

  void PLDeviceTraceLogger::addAIMEvent(uint64_t trace, double hostTimestamp)
  {
    uint64_t traceID = getTraceId(trace);

    uint32_t slot = traceID / 2;
    Monitor* mon = db->getStaticInfo().getAIMonitor(deviceId, xclbin, slot);
    if (!mon) {
      // In hardware emulation, there might be monitors inserted that
      //  don't show up in the debug ip layout.  These are added for
      //  their own debugging purposes and we should ignore any packets
      //  we see from them
      return ;
    }
    uint64_t memStrId = 0;
    if(-1 != mon->memIndex) {
      Memory* mem = db->getStaticInfo().getMemory(deviceId, mon->memIndex);
      if(nullptr != mem) {
        memStrId = db->getDynamicInfo().addString(mem->spTag);
      }
    }

    int32_t cuId = mon->cuIndex;
    VTFEventType ty = (traceID & 0x1) ? KERNEL_WRITE : KERNEL_READ;

    addKernelDataTransferEvent(ty, trace, slot, cuId, hostTimestamp, memStrId);
  }

  void PLDeviceTraceLogger::addASMEvent(uint64_t trace, double hostTimestamp)
  {
    auto traceId = getTraceId(trace);
    auto eventFlags = getEventFlags(trace);
    auto deviceTimestamp = getDeviceTimestamp(trace);
    auto slot = traceId - util::min_trace_id_asm;

    Monitor* mon  = db->getStaticInfo().getASMonitor(deviceId, xclbin, slot);
    if (!mon) {
      // In hardware emulation, there might be monitors inserted
      //  that don't show up in the debug ip layout.  These are added
      //  for their own debugging purposes and we should ignore any
      //  packets we see from them.
      return;
    }
    int32_t cuId = mon->cuIndex;

    bool isSingle    = eventFlags & 0x10;
    bool txEvent     = eventFlags & 0x8;
    bool stallEvent  = eventFlags & 0x4;
    bool starveEvent = eventFlags & 0x2;
    bool isStart     = eventFlags & 0x1;

    VTFEventType streamEventType = KERNEL_STREAM_WRITE;
    if(txEvent) {
      streamEventType =
        (mon->isStreamRead) ? KERNEL_STREAM_READ : KERNEL_STREAM_WRITE;
    } else if(starveEvent) {
      streamEventType =
        (mon->isStreamRead) ? KERNEL_STREAM_READ_STARVE : KERNEL_STREAM_WRITE_STARVE;
    } else if(stallEvent) {
      streamEventType =
        (mon->isStreamRead) ? KERNEL_STREAM_READ_STALL : KERNEL_STREAM_WRITE_STALL;
    }

    DeviceStreamAccess* strmEvent = nullptr;
    double halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;

    if(isStart) {
      // start event
      strmEvent = new DeviceStreamAccess(0, hostTimestamp, streamEventType, deviceId, slot, cuId);
      strmEvent->setDeviceTimestamp(deviceTimestamp);
      db->getDynamicInfo().addEvent(strmEvent);
      DeviceEventInfo info;
      info.type = strmEvent->getEventType();
      info.eventID = strmEvent->getEventId();
      info.hostTimestamp = strmEvent->getTimestamp();
      info.deviceTimestamp = deviceTimestamp;
      db->getDynamicInfo().markDeviceEventStart(deviceId, traceId, info);
    } else {
      DeviceEventInfo matchingStart =
        db->getDynamicInfo().matchingDeviceEventStart(deviceId, traceId, streamEventType);
      if(isSingle || matchingStart.type == UNKNOWN_EVENT) {
        // add dummy start event
        strmEvent = new DeviceStreamAccess(0, hostTimestamp, streamEventType, deviceId, slot, cuId);
        strmEvent->setDeviceTimestamp(deviceTimestamp);
        db->getDynamicInfo().addEvent(strmEvent);
        matchingStart.type = strmEvent->getEventType();
        matchingStart.eventID = strmEvent->getEventId();
        matchingStart.hostTimestamp = hostTimestamp;
        matchingStart.deviceTimestamp = deviceTimestamp;
        hostTimestamp += halfCycleTimeInMs;
      }
      // add end event
      strmEvent = new DeviceStreamAccess(matchingStart.eventID, hostTimestamp, streamEventType, deviceId, slot, cuId);
      strmEvent->setDeviceTimestamp(deviceTimestamp);
      db->getDynamicInfo().addEvent(strmEvent);
      asmLastTrans[slot] = deviceTimestamp;
    }
  }

  void PLDeviceTraceLogger::
  addKernelDataTransferEvent(VTFEventType ty,
                             uint64_t trace,
                             uint32_t slot,
                             int32_t cuId,
                             double hostTimestamp,
                             uint64_t memStrId)
  {
    DeviceMemoryAccess* memEvent = nullptr;
    double halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;
    uint64_t traceId = getTraceId(trace);
    uint64_t deviceTimestamp = getDeviceTimestamp(trace);
    uint64_t reserved = getReserved(trace);

    if (isDeviceEventTypeStart(trace)) {
      // If we see two starts in a row of the same type on the same slot,
      //  then we must have dropped an end packet.  Add a dummy end packet
      //  here.
      if (db->getDynamicInfo().hasMatchingDeviceEventStart(deviceId, traceId, ty)){
        DeviceEventInfo matchingStart =
          db->getDynamicInfo().matchingDeviceEventStart(deviceId, traceId, ty);
        memEvent =
          new DeviceMemoryAccess(matchingStart.eventID,
                                 hostTimestamp - halfCycleTimeInMs,
                                 ty, deviceId, slot, cuId,
                                 memStrId);
        memEvent->setDeviceTimestamp(deviceTimestamp);
        db->getDynamicInfo().addEvent(memEvent);
        aimLastTrans[slot] = deviceTimestamp;
      }

      memEvent = new DeviceMemoryAccess(0, hostTimestamp, ty, deviceId, slot, cuId, memStrId);
      memEvent->setDeviceTimestamp(deviceTimestamp);
      db->getDynamicInfo().addEvent(memEvent);
      DeviceEventInfo info;
      info.type = memEvent->getEventType();
      info.eventID = memEvent->getEventId();
      info.hostTimestamp = memEvent->getTimestamp();
      info.deviceTimestamp = deviceTimestamp;
      db->getDynamicInfo().markDeviceEventStart(deviceId, traceId, info);
    }
    else {
      DeviceEventInfo matchingStart =
        db->getDynamicInfo().matchingDeviceEventStart(deviceId, traceId, ty);
      if (matchingStart.type == UNKNOWN_EVENT) {
        // We need to add a dummy start event for this observed end event
        memEvent = new DeviceMemoryAccess(0, hostTimestamp, ty, deviceId, slot, cuId, memStrId);
        memEvent->setDeviceTimestamp(deviceTimestamp);
        db->getDynamicInfo().addEvent(memEvent);
        matchingStart.type = memEvent->getEventType();
        matchingStart.eventID = memEvent->getEventId();
        matchingStart.hostTimestamp = hostTimestamp;
        matchingStart.deviceTimestamp = deviceTimestamp;

        // Also, progress time so the end is after the start
        hostTimestamp += halfCycleTimeInMs;
      } else if (reserved == 1) {
        // We have a matching start, so we need to end it
        if (matchingStart.hostTimestamp == hostTimestamp) {
          // All we have to do is push time forward and let this end event
          //  match the start we found
          hostTimestamp += halfCycleTimeInMs;
        } else {
          // The times are different, so we need to end the matching start
          //  and then create an additional pulse
          memEvent = new DeviceMemoryAccess(matchingStart.eventID,
                                            hostTimestamp, ty,
                                            deviceId, slot, cuId, memStrId);
          memEvent->setDeviceTimestamp(deviceTimestamp);
          db->getDynamicInfo().addEvent(memEvent);

          // Now create the dummy start
          memEvent = new DeviceMemoryAccess(0, hostTimestamp, ty,
                                            deviceId, slot, cuId, memStrId);
          memEvent->setDeviceTimestamp(deviceTimestamp);
          db->getDynamicInfo().addEvent(memEvent);
          matchingStart.type = memEvent->getEventType();
          matchingStart.eventID = memEvent->getEventId();
          matchingStart.hostTimestamp = hostTimestamp;
          matchingStart.deviceTimestamp = deviceTimestamp;
          // Also, progress time so the end is after the start
          hostTimestamp += halfCycleTimeInMs;
        }
      }

      // The true end event we observed
      memEvent = new DeviceMemoryAccess(matchingStart.eventID,
                                        hostTimestamp, ty,
                                        deviceId, slot, cuId, memStrId);
      memEvent->setDeviceTimestamp(deviceTimestamp);
      db->getDynamicInfo().addEvent(memEvent);
      aimLastTrans[slot] = deviceTimestamp;
    }
  }

 void PLDeviceTraceLogger::addApproximateCUEndEvents()
  {
    for(uint32_t amIndex = 0; amIndex < cuStarts.size(); ++amIndex) {
      if(cuStarts[amIndex].empty()) {
        continue;
      }
      // slot : amIndex
      // start event, end event
      // start end must have created already
      // check if the memory ports on current cu has any event
      uint64_t cuLastTimestamp  = amLastTrans[amIndex];

      // get CU Id for the current slot
      Monitor* am = db->getStaticInfo().getAMonitor(deviceId, xclbin, amIndex);
      int32_t  cuId = am->cuIndex;

      // Check if any memory port on current CU had a trace packet
      for(uint64_t aimIndex = 0; aimIndex < aimLastTrans.size(); ++aimIndex) {
        // To reduce overhead, first check the timestamp.
        // If last activity timestamp on CU is earlier than current AIM, then only check
        // whether the current AIM is attached to the same CU.
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
        // To reduce overhead, first check the timestamp.
        // If last activity timestamp on CU is earlier than current ASM, then only check
        // whether the current ASM is attached to the same CU.
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
      if (!warnCUIncomplete) {
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
        warnCUIncomplete = true;
      }

      // end event
      double hostTimestamp = convertDeviceToHostTimestamp(cuLastTimestamp);
      addCUEndEvent(hostTimestamp, cuLastTimestamp, amIndex, cuId);
    }
  }

  void
  PLDeviceTraceLogger::addApproximateDataTransferEvent(VTFEventType type,
                                                       uint64_t aimTraceID,
                                                       int32_t amId,
                                                       int32_t cuId,
                                                       uint64_t memStrId)
  {
    DeviceEventInfo startEvent =
      db->getDynamicInfo().matchingDeviceEventStart(deviceId, aimTraceID, type);
    if (startEvent.type == UNKNOWN_EVENT)
      return;

    uint64_t transStartTimestamp = 0;
    uint64_t transApproxEndTimestamp = 0;
    double transApproxEndHostTimestamp = 0;

    const double halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;

    transStartTimestamp = startEvent.deviceTimestamp;
    if (amId == -1) {
      // This is a floating AIM monitor not attached to any particular CU.
      transApproxEndTimestamp = transStartTimestamp;
      transApproxEndHostTimestamp = convertDeviceToHostTimestamp(transStartTimestamp) + halfCycleTimeInMs;
    }
    else {
      // Check the last known transaction on the CU to approximate the end
      uint64_t cuLastTimestamp  = amLastTrans[amId];
      if (transStartTimestamp < cuLastTimestamp) {
        transApproxEndTimestamp = cuLastTimestamp;
        transApproxEndHostTimestamp = convertDeviceToHostTimestamp(cuLastTimestamp);
      }
      else {
        transApproxEndTimestamp = transStartTimestamp;
        transApproxEndHostTimestamp = convertDeviceToHostTimestamp(transStartTimestamp) + halfCycleTimeInMs;
      }
    }
    // Add approximate end event
    DeviceMemoryAccess* endEvent =
      new DeviceMemoryAccess(startEvent.eventID,
                             transApproxEndHostTimestamp,
                             type,
                             deviceId, amId, cuId, memStrId);
    endEvent->setDeviceTimestamp(transApproxEndTimestamp);
    db->getDynamicInfo().addEvent(endEvent);
  }

  void PLDeviceTraceLogger::addApproximateDataTransferEndEvents()
  {
    // Go through all of our AIMs that have trace enabled.  If any of them
    //  have any outstanding reads or writes, then finish them based off of
    //  the last CU execution time.
    auto aims = db->getStaticInfo().getUserAIMsWithTrace(deviceId, xclbin);

    // aims is a vector of Monitor*.
    for (auto mon : aims) {
      if (!mon)
        continue ;
      // We can get the trace IDs of what the hardware packets would
      //  be by (slotIndex * 2) for read packets and (slotIndex * 2) + 1
      //  for write packets
      uint64_t aimReadId = mon->slotIndex * 2;
      uint64_t aimWriteId = (mon->slotIndex * 2) + 1;

      int32_t cuId = mon->cuIndex;
      int32_t amId = -1;

      uint64_t memStrId = 0;
      if(-1 != mon->memIndex) {
        Memory* mem = db->getStaticInfo().getMemory(deviceId, mon->memIndex);
        if(nullptr != mem) {
          memStrId = db->getDynamicInfo().addString(mem->spTag);
        }
      }

      if (cuId != -1) {
        ComputeUnitInstance* cu = db->getStaticInfo().getCU(deviceId, cuId);
        if (cu) {
          amId = cu->getAccelMon();
        }
      }

      addApproximateDataTransferEvent(KERNEL_READ, aimReadId, amId, cuId, memStrId);
      addApproximateDataTransferEvent(KERNEL_WRITE, aimWriteId, amId, cuId, memStrId);
    }
  }

  void PLDeviceTraceLogger::addApproximateDataTransferEndEvents(int32_t cuId)
  {
    if (cuId == -1)
      return;
    for (uint64_t aimIndex = 0;
         aimIndex < (db->getStaticInfo()).getNumAIM(deviceId, xclbin);
         ++aimIndex) {

      uint64_t aimSlotID = (aimIndex * 2) + util::min_trace_id_aim;
      Monitor* mon =
        db->getStaticInfo().getAIMonitor(deviceId, xclbin, aimIndex);
      if (!mon)
        continue;

      if (cuId != mon->cuIndex)
        continue;

      uint64_t memStrId = 0;
      if(-1 != mon->memIndex) {
        Memory* mem = db->getStaticInfo().getMemory(deviceId, mon->memIndex);
        if(nullptr != mem) {
          memStrId = db->getDynamicInfo().addString(mem->spTag);
        }
      }

      int32_t amId = -1;
      ComputeUnitInstance* cu = db->getStaticInfo().getCU(deviceId, cuId);
      if (cu) {
        amId = cu->getAccelMon();
      }
      addApproximateDataTransferEvent(KERNEL_READ, aimSlotID, amId, cuId, memStrId);
      addApproximateDataTransferEvent(KERNEL_WRITE, aimSlotID + 1, amId, cuId, memStrId);
    }
  }

  void PLDeviceTraceLogger::addApproximateStreamEndEvents()
  {
    // Find unfinished ASM events
    bool unfinishedASMevents = false;
    for(uint64_t asmIndex = 0; asmIndex < (db->getStaticInfo()).getNumUserASMWithTrace(deviceId, xclbin); ++asmIndex) {
      uint64_t asmTraceID = asmIndex + util::min_trace_id_asm;
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

      VTFEventType streamEventType = (mon->isStreamRead) ? KERNEL_STREAM_READ : KERNEL_STREAM_WRITE;
      addApproximateStreamEndEvent(asmIndex, asmTraceID, streamEventType, cuId, amId, cuLastTimestamp, asmAppxLastTransTimeStamp, unfinishedASMevents);

      streamEventType = (mon->isStreamRead) ? KERNEL_STREAM_READ_STALL : KERNEL_STREAM_WRITE_STALL;
      addApproximateStreamEndEvent(asmIndex, asmTraceID, streamEventType, cuId, amId, cuLastTimestamp, asmAppxLastTransTimeStamp, unfinishedASMevents);

      streamEventType = (mon->isStreamRead) ? KERNEL_STREAM_READ_STARVE : KERNEL_STREAM_WRITE_STARVE;
      addApproximateStreamEndEvent(asmIndex, asmTraceID, streamEventType, cuId, amId, cuLastTimestamp, asmAppxLastTransTimeStamp, unfinishedASMevents);

      asmLastTrans[asmIndex] = asmAppxLastTransTimeStamp;
    }

    if(unfinishedASMevents) {
      const char* msg = "Found unfinished events on Stream connections. Adding approximate ends for Stream Activity/Stall/Starve on timeline trace.";
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
    }
  }

  void PLDeviceTraceLogger::addApproximateStallEndEvents(uint64_t trace, double hostTimestamp, uint32_t s, uint64_t monTraceID, int32_t cuId)
  {
    if (traceIDs[s] == 0)
      return;
    // There are some stall events still outstanding that need to be closed
    const double halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;

    if (traceIDs[s] & STALL_INT_MASK) {
      addStallEvent(trace, hostTimestamp-halfCycleTimeInMs, s, monTraceID, cuId,
                    KERNEL_STALL_DATAFLOW, STALL_INT_MASK);
    }
    if (traceIDs[s] & STALL_STR_MASK) {
      addStallEvent(trace, hostTimestamp-halfCycleTimeInMs, s, monTraceID,
                    cuId, KERNEL_STALL_PIPE, STALL_STR_MASK);
    }
    if (traceIDs[s] & STALL_EXT_MASK) {
      addStallEvent(trace, hostTimestamp-halfCycleTimeInMs, s, monTraceID, cuId,
                    KERNEL_STALL_EXT_MEM, STALL_EXT_MASK);
    }
  }

  void PLDeviceTraceLogger::addApproximateStreamEndEvent(uint64_t asmIndex, uint64_t asmTraceID, VTFEventType streamEventType,
                                                                 int32_t cuId, int32_t  amId, uint64_t cuLastTimestamp,
                                                                 uint64_t &asmAppxLastTransTimeStamp, bool &unfinishedASMevents)
  {
    uint64_t asmStartTimestamp = 0, asmAppxEndTimestamp = 0;
    double   asmAppxEndHostTimestamp = 0;
    double   halfCycleTimeInMs = (0.5/traceClockRateMHz)/1000.0;

    DeviceEventInfo matchingStart =
      db->getDynamicInfo().matchingDeviceEventStart(deviceId, asmTraceID,streamEventType);
    while(matchingStart.type != UNKNOWN_EVENT) {
      unfinishedASMevents = true;
      asmStartTimestamp = matchingStart.deviceTimestamp;
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
      DeviceStreamAccess* strmEvent = new DeviceStreamAccess(matchingStart.eventID, asmAppxEndHostTimestamp,
                                                           streamEventType, deviceId, asmIndex, cuId);
      strmEvent->setDeviceTimestamp(asmAppxEndTimestamp);
      db->getDynamicInfo().addEvent(strmEvent);

      matchingStart = db->getDynamicInfo().matchingDeviceEventStart(deviceId, asmTraceID, streamEventType);
    }
  }

  // Clock training packets in hardware have pairs of device timestamps and
  // corresponding host timestamps.  We need at least two training packets
  // to plot a line and get the slopes we use for adjusting timestamps.
  // As the device progresses, we'll encounter additional training packets and
  // they may not be continuous, so this function uses static variables
  // to keep track of the last packet we've seen.
  void PLDeviceTraceLogger::trainDeviceHostTimestamps(uint64_t deviceTimestamp, uint64_t hostTimestamp)
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
  double PLDeviceTraceLogger::convertDeviceToHostTimestamp(uint64_t deviceTimestamp)
  {
    return ((clockTrainSlope * (double)deviceTimestamp) + clockTrainOffset)/1e6;
  }

  void PLDeviceTraceLogger::processTraceData(void* data, uint64_t numBytes)
  {
    if (numBytes == 0)
      return;
    if (!VPDatabase::alive())
      return;

    uint64_t numPackets = numBytes / sizeof(uint64_t);
    uint64_t start = 0;

    // Try to find 8 contiguous clock training packets.  Anything before that
    //  is garbage from the previous run
    // Note: This needs to be done only in beginning chunk of data
    static bool found = false;
    if (!found) {
      for (uint64_t i = 0; i < numPackets - 8; ++i) {
        for (uint64_t j = i; j < i + 8; ++j) {
          uint64_t packet = (static_cast<uint64_t*>(data))[j];
          if (!isClockTraining(packet))
            break;
          if (j == (i + 7)) {
            start = i ;
            found = true;
          }
        }
        if (found)
          break;
      }
    }

    // Clock Training state is preserved across calls
    static uint32_t modulus = 0;
    static uint64_t clockTrainingHostTimestamp = 0;

    for (uint64_t i = start ; i < numPackets ; ++i) {
      uint64_t packet = (static_cast<uint64_t*>(data))[i];
      auto deviceTimestamp = getDeviceTimestamp(packet);
      auto traceId = getTraceId(packet);
      auto clockTrainingDeviceTimestamp = deviceTimestamp;

      /*
      if (i == start && firstTimestamp == 0) {
        firstTimestamp = deviceTimestamp ;
      }
      */

      if (isClockTraining(packet)) {
        if (modulus == 0) {
          if (clockTrainingDeviceTimestamp >= firstTimestamp) {
            clockTrainingDeviceTimestamp =
              clockTrainingDeviceTimestamp - firstTimestamp;
          }
          else {
            clockTrainingDeviceTimestamp =
              clockTrainingDeviceTimestamp + (0x1FFFFFFFFFFF - firstTimestamp);
          }
        }
        clockTrainingHostTimestamp |= ((packet >> 45) & 0xFFFF) << (16 * modulus);
        ++modulus;
        if (modulus == 4) {
          // It requires four complete clock training packets before
          //  we can perform the clock training algorithm
          trainDeviceHostTimestamps(clockTrainingDeviceTimestamp,
                                    clockTrainingHostTimestamp);
          clockTrainingHostTimestamp = 0;
          clockTrainingDeviceTimestamp = 0;
          modulus = 0;
        }
        continue;
      }

      bool AMPacket  = (traceId >= util::min_trace_id_am &&
                        traceId <= util::max_trace_id_am);
      bool AIMPacket = (traceId <= util::max_trace_id_aim); // min trace id aim == 0
      bool ASMPacket = (traceId >= util::min_trace_id_asm &&
                        traceId <  util::max_trace_id_asm);
      if (!AMPacket && !AIMPacket && !ASMPacket) {
        continue;
      }

      double hostTimestamp = convertDeviceToHostTimestamp(deviceTimestamp);
      if (AMPacket) {
        addAMEvent(packet, hostTimestamp);
      }
      if (AIMPacket) {
        addAIMEvent(packet, hostTimestamp);
      }
      if (ASMPacket) {
        addASMEvent(packet, hostTimestamp);
      }

      // keep track of latest timestamp that comes through trace
      mLatestHostTimestampMs = hostTimestamp;
    }

  }

  void PLDeviceTraceLogger::endProcessTraceData()
  {
    addApproximateCUEndEvents();
    addApproximateDataTransferEndEvents();
    addApproximateStreamEndEvents();
  }

  void PLDeviceTraceLogger::addEventMarkers(bool isFIFOFull, bool isTS2MMFull)
  {
    // User event API takes ns as time
    std::chrono::nanoseconds mark_time(static_cast<uint64_t>(mLatestHostTimestampMs * 1e6));

    if (isFIFOFull) {
      xrt::profile::user_event events;
      events.mark_time_ns(mark_time, "Device Trace FIFO Full");
    }

    if (isTS2MMFull) {
        xrt::profile::user_event events;
        events.mark_time_ns(mark_time, "Device Trace Buffer Full");
    }
  }

} // end namespace xdp

