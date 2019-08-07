/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "trace_logger.h"
#include "rt_util.h"
#include "xdp/profile/config.h"
#include "xdp/profile/collection/results.h"
#include "xdp/profile/collection/counters.h"
#include "xdp/profile/device/trace_parser.h"
#include "xdp/profile/writer/base_profile.h"
#include "xdp/profile/writer/base_trace.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <cassert>

namespace xdp {
  // ************************
  // XDP Profile TraceLogger Class
  // ************************
  TraceLogger::TraceLogger(ProfileCounters* profileCounters, TraceParser * TraceParserHandle, XDPPluginI* Plugin)
  : mMigrateMemCalls(0),
    mHostP2PTransfers(0),
    mCurrentContextId(0),
    mCuStarts(0),
    mProfileCounters(profileCounters),
    mTraceParserHandle(TraceParserHandle),
    mPluginHandle(Plugin)
  {
  }

  TraceLogger::~TraceLogger()
  {
    mKernelTraceMap.clear();
    mBufferTraceMap.clear();
    mDeviceTraceMap.clear();
    mKernelStartsMap.clear();
  }

  // ***************************************************************************
  // Helpers
  // ***************************************************************************

  // Get a device timestamp
  double TraceLogger::getDeviceTimeStamp(double hostTimeStamp, const std::string& deviceName)
  {
    double deviceTimeStamp = hostTimeStamp;

    // In HW emulation, use estimated host timestamp based on device clock cycles (in psec from HAL)
    if (mPluginHandle->getFlowMode() == xdp::RTUtil::HW_EM) {
      size_t dts = mPluginHandle->getDeviceTimestamp(deviceName);
      deviceTimeStamp = dts / 1000000.0;
    }

    return deviceTimeStamp;
  }

  // Attach new trace writer
  void TraceLogger::attach(TraceWriterI* writer)
  {
    std::lock_guard < std::mutex > lock(mLogMutex);
    auto itr = std::find(mTraceWriters.begin(), mTraceWriters.end(), writer);
    if (itr == mTraceWriters.end())
      mTraceWriters.push_back(writer);
  }

  // Detach new trace writer
  void TraceLogger::detach(TraceWriterI* writer)
  {
    std::lock_guard < std::mutex > lock(mLogMutex);
    auto itr = std::find(mTraceWriters.begin(), mTraceWriters.end(), writer);
    if (itr != mTraceWriters.end())
      mTraceWriters.erase(itr);
  }

  // ***************************************************************************
  // Timeline trace writers
  // ***************************************************************************

  // Write API call events to trace
  void TraceLogger::writeTimelineTrace( double traceTime,
      const char* functionName, const char* eventName, unsigned int functionID) const
  {
    //if (!this->isTimelineTraceFileOn())
    //  return;

    for (auto w : mTraceWriters) {
      w->writeFunction(traceTime, functionName, eventName, functionID);
    }
  }

  // Write kernel event to trace
  void TraceLogger::writeTimelineTrace( double traceTime,
      const std::string& commandString, const std::string& stageString,
      const std::string& eventString, const std::string& dependString,
      uint64_t objId, size_t size) const
  {
    //if (!this->isTimelineTraceFileOn())
    //  return;

    for (auto w : mTraceWriters) {
      w->writeKernel(traceTime, commandString, stageString, eventString,
                     dependString, objId, size);
    }
  }

  // Write cu event to trace
  void TraceLogger::writeTimelineTrace( double traceTime,
      const std::string& commandString, const std::string& stageString,
      const std::string& eventString, const std::string& dependString,
      uint64_t objId, size_t size, uint32_t cuId) const
  {
    for (auto w : mTraceWriters) {
      w->writeCu(traceTime, commandString, stageString, eventString,
                     dependString, objId, size, cuId);
    }
  }

  // Write data transfer event to trace
  void TraceLogger::writeTimelineTrace(double traceTime, RTUtil::e_profile_command_kind kind,
      const std::string& commandString, const std::string& stageString,
      const std::string& eventString, const std::string& dependString,
      size_t size, uint64_t srcAddress, const std::string& srcBank,
      uint64_t dstAddress, const std::string& dstBank,
      std::thread::id threadId) const
  {
    //if (!this->isTimelineTraceFileOn())
    //  return;

    for (auto w : mTraceWriters) {
      w->writeTransfer(traceTime, kind, commandString, stageString, eventString, dependString,
                       size, srcAddress, srcBank, dstAddress, dstBank, threadId);
    }
  }

  // Write Dependency information
  void TraceLogger::writeTimelineTrace(double traceTime,
      const std::string& commandString, const std::string& stageString,
      const std::string& eventString, const std::string& dependString) const
  {
    //if (!this->isTimelineTraceFileOn())
    //  return;

    for (auto w : mTraceWriters) {
      w->writeDependency(traceTime, commandString, stageString, eventString, dependString);
    }
  }

  // ***************************************************************************
  // Log host function calls (e.g., OpenCL APIs)
  // ***************************************************************************

  void TraceLogger::logFunctionCallStart(const char* functionName, long long queueAddress, unsigned int functionID)
  {
    double timeStamp = mPluginHandle->getTraceTime();

    std::string name(functionName);
    if (name.find("MigrateMem") != std::string::npos)
      mMigrateMemCalls++;

    if (queueAddress == 0)
      name += "|General";
    else
      (name += "|") +=std::to_string(queueAddress);
    std::lock_guard<std::mutex> lock(mLogMutex);
    mProfileCounters->logFunctionCallStart(functionName, timeStamp);
    writeTimelineTrace(timeStamp, name.c_str(), "START", functionID);
    mFunctionStartLogged = true;

#if 0
    // Write host event to trace buffer
    xclPerfMonEventID eventID = RTUtil::getFunctionEventID(name, queueAddress);
    if (eventID != XCL_PERF_MON_IGNORE_EVENT) {
      xclPerfMonEventType eventType = XCL_PERF_MON_START_EVENT;
      xdp::profile::platform::write_host_event(xdp::RTSingleton::Instance()->getcl_platform_id(), eventType, eventID);
    }
#endif
  }

  void TraceLogger::logFunctionCallEnd(const char* functionName, long long queueAddress, unsigned int functionID)
  {
    // Log function call start if not done so already
    // NOTE: this addresses a race condition when constructing the singleton (CR 963297)
    if (!mFunctionStartLogged)
      logFunctionCallStart(functionName, queueAddress, functionID);

    double timeStamp = mPluginHandle->getTraceTime();

    std::string name(functionName);
    if (queueAddress == 0)
      name += "|General";
    else
      (name += "|") +=std::to_string(queueAddress);

    std::lock_guard<std::mutex> lock(mLogMutex);
    mProfileCounters->logFunctionCallEnd(functionName, timeStamp);
    writeTimelineTrace(timeStamp, name.c_str(), "END", functionID);

#if 0
    // Write host event to trace buffer
    xclPerfMonEventID eventID = RTUtil::getFunctionEventID(name, queueAddress);
    if (eventID != XCL_PERF_MON_IGNORE_EVENT) {
      xclPerfMonEventType eventType = XCL_PERF_MON_END_EVENT;
      xdp::profile::platform::write_host_event(xdp::RTSingleton::Instance()->getcl_platform_id(), eventType, eventID);
    }
#endif
  }

  // ***************************************************************************
  // Log Host Data Transfers
  // ***************************************************************************

  void TraceLogger::logDataTransfer(uint64_t objId, RTUtil::e_profile_command_kind objKind,
      RTUtil::e_profile_command_state objStage, size_t objSize, uint32_t contextId,
      uint32_t numDevices, std::string deviceName, uint32_t commandQueueId,
      uint64_t srcAddress, const std::string& srcBank, uint64_t dstAddress, const std::string& dstBank,
      std::thread::id threadId, const std::string eventString, const std::string dependString,
      double timeStampMsec)
  {
    double timeStamp = (timeStampMsec > 0.0) ? timeStampMsec :
        mPluginHandle->getTraceTime();

    std::string commandString;
    std::string stageString;
    std::lock_guard < std::mutex > lock(mLogMutex);
    RTUtil::commandKindToString(objKind, commandString);
    RTUtil::commandStageToString(objStage, stageString);

    // Collect time trace
    BufferTrace* traceObject = nullptr;
    auto itr = mBufferTraceMap.find(objId);
    if (itr == mBufferTraceMap.end()) {
      traceObject = BufferTrace::reuse();
      mBufferTraceMap[objId] = traceObject;
    }
    else {
      traceObject = itr->second;
    }
    RTUtil::setTimeStamp(objStage, traceObject, timeStamp);

    // clEnqueueNDRangeKernel returns END with no START
    // if data transfer was already completed.
    // We can safely discard those events
    if (objStage == RTUtil::END && (traceObject->getStart() > 0.0)) {
      // Collect performance counters
      mProfileCounters->logBufferTransfer(objKind, objSize, (traceObject->End - traceObject->Start), contextId, numDevices);

      if (objKind == RTUtil::READ_BUFFER || objKind == RTUtil::WRITE_BUFFER) {
        bool isRead = (objKind == RTUtil::READ_BUFFER);
        mProfileCounters->pushToSortedTopUsage(traceObject, isRead);
      }
      else if (objKind == RTUtil::READ_BUFFER_P2P || objKind == RTUtil::WRITE_BUFFER_P2P)
        mHostP2PTransfers++;

      // Mark and keep top trace data
      // Data can be additionally streamed to a data transfer record
      traceObject->Address = srcAddress;
      traceObject->Size = objSize;
      traceObject->ContextId = contextId;
      traceObject->CommandQueueId = commandQueueId;
      auto itr = mBufferTraceMap.find(objId);
      mBufferTraceMap.erase(itr);

      // Store thread IDs into set
      addToThreadIds(threadId);
    }

    writeTimelineTrace(timeStamp, objKind, commandString, stageString, eventString, dependString,
                       objSize, srcAddress, srcBank, dstAddress, dstBank, threadId);

#if 0
    // Write host event to trace buffer
    if (objStage == RTUtil::START || objStage == RTUtil::END) {
      xclPerfMonEventType eventType = (objStage == RTUtil::START) ? XCL_PERF_MON_START_EVENT : XCL_PERF_MON_END_EVENT;
      xclPerfMonEventID eventID = (objKind == RTUtil::READ_BUFFER) ? XCL_PERF_MON_READ_ID : XCL_PERF_MON_WRITE_ID;
      xdp::profile::platform::write_host_event(xdp::RTSingleton::Instance()->getcl_platform_id(), eventType, eventID);
    }
#endif
  }

  // ***************************************************************************
  // Log Kernel execution
  // ***************************************************************************

  //an empty cu_name indicates its doing original "kernel" profiling
  //a non empty call implies we need to collect compute unit based info.
  //Both will be called for a run, since we need to collect/display both
  //  kernel as well as compute unit info.
  void TraceLogger::logKernelExecution(uint64_t objId, uint32_t programId, uint64_t eventId,
      RTUtil::e_profile_command_state objStage, std::string kernelName, std::string xclbinName,
      uint32_t contextId, uint32_t commandQueueId, const std::string& deviceName, uid_t uid,
      const size_t* globalWorkSize, size_t workGroupSize, const size_t* localWorkDim,
      const std::string& cu_name, const std::string eventString, const std::string dependString,
      double timeStampMsec)
  {
    double timeStamp = (timeStampMsec > 0.0) ? timeStampMsec :
      mPluginHandle->getTraceTime();

    if (mGetFirstCUTimestamp && (objStage == RTUtil::START)) {
      auto tp = mTraceParserHandle;
      tp->setStartTimeMsec(timeStamp);
      mGetFirstCUTimestamp = false;
    }

    std::lock_guard<std::mutex> lock(mLogMutex);

    // TODO: create unique name for device since currently all devices are called fpga0
    // NOTE: see also logCounters for corresponding device name for counters
    std::string newDeviceName = deviceName + "-" + std::to_string(uid);

    // In HW emulation, use estimated host timestamp based on device clock cycles
    double deviceTimeStamp = getDeviceTimeStamp(timeStamp, newDeviceName);

    // Placeholders for ID and name used in device trace reporting
    // TODO: need to grab actual kernel name and context ID from AXI IDs and metadata
    mCurrentContextId = contextId;
    mCurrentKernelName = kernelName;
    mCurrentDeviceName = newDeviceName;
    mCurrentBinaryName = xclbinName;

    std::string commandString;
    std::string stageString;
    RTUtil::commandKindToString(RTUtil::EXECUTE_KERNEL, commandString);
    RTUtil::commandStageToString(objStage, stageString);

    std::string cuName("");

    std::string globalSize = std::to_string(globalWorkSize[0]) + ":" +
        std::to_string(globalWorkSize[1]) + ":" + std::to_string(globalWorkSize[2]);
    std::string localSize = std::to_string(localWorkDim[0]) + ":" +
        std::to_string(localWorkDim[1]) + ":" + std::to_string(localWorkDim[2]);

    // *******
    // Kernels
    // *******
    if (cu_name.empty()) {
      // Collect stats for max/min/average kernel times
      // NOTE: create unique kernel name using object ID
      std::string newKernelName = kernelName + "|" + std::to_string(objId) + "|"  + std::to_string(programId);
      if (objStage == RTUtil::START) {
        // Queue STARTS because events come in async order
        mKernelStartsMap[newKernelName].push(deviceTimeStamp);
        XDP_LOG("logKernelExecution: kernel START @ %.3f msec for %s\n", deviceTimeStamp, newKernelName.c_str());
      }
      else if (objStage == RTUtil::END) {
        auto it = mKernelStartsMap.find(newKernelName);
        if (it != mKernelStartsMap.end() && !it->second.empty()) {
          XDP_LOG("logKernelExecution: kernel END @ %.3f msec for %s\n", deviceTimeStamp, newKernelName.c_str());
          mProfileCounters->logKernelExecutionStart(newKernelName, newDeviceName, it->second.front());
          mProfileCounters->logKernelExecutionEnd(newKernelName, newDeviceName, deviceTimeStamp);
          it->second.pop();
        }
      }

      // Collect trace objects
      KernelTrace* traceObject = nullptr;
      auto itr = mKernelTraceMap.find(eventId);
      if(itr == mKernelTraceMap.end()) {
        traceObject = KernelTrace::reuse();
        mKernelTraceMap[eventId] = traceObject;
      } else {
        traceObject = itr->second;
      }
      RTUtil::setTimeStamp(objStage, traceObject, deviceTimeStamp);
      if (objStage == RTUtil::END) {
        traceObject->Address = objId;
        traceObject->ContextId = contextId;
        traceObject->CommandQueueId = commandQueueId;
        traceObject->KernelName = kernelName;
        traceObject->DeviceName = newDeviceName;
        traceObject->WorkGroupSize = workGroupSize;
        traceObject->GlobalWorkSize[0] = globalWorkSize[0];
        traceObject->GlobalWorkSize[1] = globalWorkSize[1];
        traceObject->GlobalWorkSize[2] = globalWorkSize[2];
        traceObject->LocalWorkSize[0] = localWorkDim[0];
        traceObject->LocalWorkSize[1] = localWorkDim[1];
        traceObject->LocalWorkSize[2] = localWorkDim[2];

        auto itr = mKernelTraceMap.find(eventId);
        mKernelTraceMap.erase(itr);
        // Only log Valid trace objects
        if (traceObject->getStart() > 0.0 && traceObject->getStart() < deviceTimeStamp) {
          mProfileCounters->pushToSortedTopUsage(traceObject);
        }
      }

      // Write all states to timeline trace
      std::string uniqueCUName("KERNEL|");
      uniqueCUName += newDeviceName + "|" + xclbinName + "|" + kernelName + "|" + localSize + "|all";
      commandString = uniqueCUName;
      writeTimelineTrace(timeStamp, commandString, stageString, eventString, dependString,
                         objId, workGroupSize);
    }
    //
    // Compute Units
    //
    else {
      /*
       * log cu stats per device + xclbin + programID
       * IN HW_EMU the monitors aren't reset even on xclbin change
       * i.e counters for same xclbin accumulate for every program ID
       * IN HW the monitors are initialied to 0 for every xclbin load
       * so counter data is unique for every program ID + xclbin combination
       */
      std::string uniqueCuDataKey;
      uint32_t cuId = 0;
      if (mPluginHandle->getFlowMode() == xdp::RTUtil::DEVICE) {
        uniqueCuDataKey = xclbinName + std::to_string(programId);
      } else {
        uniqueCuDataKey = xclbinName + std::to_string(0);
      }
      // Naming used in profile summary
      cuName = newDeviceName + "|" + kernelName + "|" + globalSize + "|" + localSize
               + "|" + cu_name + "|" + uniqueCuDataKey;
      if (objStage == RTUtil::START) {
        XDP_LOG("logKernelExecution: CU START @ %.3f msec for %s\n", deviceTimeStamp, cuName.c_str());
        if (mPluginHandle->getFlowMode() == xdp::RTUtil::CPU) {
          mProfileCounters->logComputeUnitExecutionStart(cuName, deviceTimeStamp);
          mProfileCounters->logComputeUnitDeviceStart(newDeviceName, timeStamp);
          cuId = ++mCuStarts;
          mCuStartsMap[objId].push(cuId);
        }
      }
      else if (objStage == RTUtil::END) {
        XDP_LOG("logKernelExecution: CU END @ %.3f msec for %s\n", deviceTimeStamp, cuName.c_str());
        // This is updated through HAL
        if (mPluginHandle->getFlowMode() != xdp::RTUtil::CPU) {
          deviceTimeStamp = 0;
        } else {
          // Find CU Start for this End
          auto &queue = mCuStartsMap[objId];
          if (!queue.empty()) {
            cuId = queue.front();
            queue.pop();
          }
        }
        mProfileCounters->logComputeUnitExecutionEnd(cuName, deviceTimeStamp);
      }

      // Naming used in timeline trace
      std::string cuName2 = kernelName + "|" + localSize + "|" + cu_name;
      std::string uniqueCUName("KERNEL|");
      (uniqueCUName += newDeviceName) += "|";
      (uniqueCUName += xclbinName) += "|";
      (uniqueCUName += cuName2) += "|";

      if (mPluginHandle->getFlowMode() == xdp::RTUtil::CPU && cuId) {
        writeTimelineTrace(timeStamp, uniqueCUName, stageString, eventString, dependString,
                           objId, workGroupSize, cuId);
      }
    }

#if 0
    // Write host event to trace buffer (only if used)
    if (objStage == RTUtil::START || objStage == RTUtil::END) {
      xclPerfMonEventType eventType = (objStage == RTUtil::START) ? XCL_PERF_MON_START_EVENT : XCL_PERF_MON_END_EVENT;
      xclPerfMonEventID eventID = (cu_name.empty()) ? XCL_PERF_MON_KERNEL0_ID : XCL_PERF_MON_CU0_ID;
      xdp::profile::platform::write_host_event(xdp::RTSingleton::Instance()->getcl_platform_id(), eventType, eventID);
    }
#endif
  }

  // ***************************************************************************
  // Log a dependency (e.g., a kernel waiting on a host write)
  // ***************************************************************************

  void TraceLogger::logDependency(RTUtil::e_profile_command_kind objKind,
      const std::string& eventString, const std::string& dependString)
  {
    std::string commandString;
    std::lock_guard < std::mutex > lock(mLogMutex);
    RTUtil::commandKindToString(objKind, commandString);

    double traceTime = mPluginHandle->getTraceTime();
    writeTimelineTrace(traceTime, commandString, "", eventString, dependString);
  }

  // ***************************************************************************
  // Log device trace
  // ***************************************************************************

  void TraceLogger::logDeviceTrace(const std::string& deviceName, const std::string& binaryName,
      xclPerfMonType type, xclTraceResultsVector& traceVector, bool endLog) {
    auto tp = mTraceParserHandle;
    if (tp == NULL || traceVector.mLength == 0)
      return;

    std::lock_guard<std::mutex> lock(mLogMutex);
    TraceParser::TraceResultVector resultVector;
    tp->logTrace(deviceName, type, traceVector, resultVector);
    if (endLog)
      tp->endLogTrace(deviceName, type, resultVector);

    if (resultVector.empty())
      return;

    // Log for summary purposes
    //uint64_t index = 0;
    for (auto it = resultVector.begin(); it != resultVector.end(); it++) {
      DeviceTrace* tr = DeviceTrace::reuse();

      // Copy trace results
      // TODO: replace with actual device and kernel names (interpreted from AXI IDs)
      tr->DeviceName = deviceName;
      tr->Name = mCurrentKernelName;
      tr->ContextId = mCurrentContextId;
      tr->SlotNum = (*it).SlotNum;
      tr->Type = (*it).Type;
      tr->Kind = (*it).Kind;
      tr->BurstLength = (*it).BurstLength;
      tr->NumBytes = (*it).NumBytes;
      tr->StartTime = (*it).StartTime;
      tr->EndTime = (*it).EndTime;
      tr->TraceStart = (*it).TraceStart;
      tr->Start = (*it).Start;
      tr->End = (*it).End;

      double durationMsec = tr->End - tr->Start;

      // Log trace results
      bool isKernel = (tr->Type.find("Kernel") != std::string::npos);
      bool isRead = (tr->Type == "Read");
      bool isKernelTransfer = (tr->Kind == DeviceTrace::DEVICE_KERNEL);
      mProfileCounters->logDeviceEvent(tr->DeviceName, tr->Name, tr->NumBytes, durationMsec,
          tp->getGlobalMemoryBitWidth(), tp->getGlobalMemoryClockFreqMHz(),
          isKernel, isRead, isKernelTransfer);
      mProfileCounters->pushToSortedTopUsage(tr, isRead, isKernelTransfer);
    }

    // Write trace results vector to files
    //if (this->isTimelineTraceFileOn()) {
      for (auto w : mTraceWriters) {
        w->writeDeviceTrace(resultVector, deviceName, binaryName);
      }
    //}

    resultVector.clear();
  }



} // xdp
