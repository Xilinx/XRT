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

#include "rt_profile.h"
#include "rt_profile_xocl.h"
#include "rt_profile_results.h"
#include "rt_profile_writers.h"
#include "rt_profile_device.h"
#include "rt_profile_rule_checks.h"
#include "rt_perf_counters.h"
#include "xdp/rt_singleton.h"
#include "debug.h"

//#include <CL/opencl.h>
#include "xocl/core/device.h"
#include "xocl/xclbin/xclbin.h"
#include "../../driver/include/xclperf.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <cassert>

// Uncomment to use device-based timestamps in timeline trace
//#define USE_DEVICE_TIMELINE

namespace XCL {
  // ***********************
  // Top-Level Profile Class
  // ***********************
  RTProfile::RTProfile(int& flag)
  : ProfileFlags(flag),
    PerfCounters(),
    FileFlags(0),
    CurrentContextId(0),
    MigrateMemCalls(0),
    FunctionStartLogged(false),
    DeviceTraceOption(DEVICE_TRACE_OFF),
    StallTraceOption(STALL_TRACE_OFF)
  {
    // Create device profiler (may or may not be used during given run)
    DeviceProfile = new RTProfileDevice();

    // Profile rule checks
    RuleChecks = new ProfileRuleChecks();
    
    memset(&CUPortsToDDRBanks, 0, MAX_DDR_BANKS*sizeof(int));

    // Indeces are now same for HW and emulation
    OclSlotIndex  = XPAR_SPM0_FIRST_KERNEL_SLOT;
    HostSlotIndex = XPAR_SPM0_HOST_SLOT;
  }

  RTProfile::~RTProfile()
  {
    if (ProfileFlags)
      writeProfileSummary();

    if (DeviceProfile != nullptr)
      delete DeviceProfile;

    if (RuleChecks != nullptr)
      delete RuleChecks;

    FinalCounterResultsMap.clear();
    RolloverCounterResultsMap.clear();
    RolloverCountsMap.clear();
  }

  double RTProfile::getTraceTime() {
    auto nsec = xrt::time_ns();
    return getTimestampMsec(nsec);

#if 0
#if 1
    struct timespec now;
    int err;
    if ((err = clock_gettime(CLOCK_MONOTONIC, &now)) < 0)
      return 0.0;

    uint64_t nsec = (uint64_t) now.tv_sec * 1000000000UL + (uint64_t) now.tv_nsec;
    return getTimestampMsec(nsec);
  #else
    using namespace std::chrono;
    typedef duration<uint64_t, std::ratio<1, 1000000000>> duration_ns;
    duration_ns time_span =
        duration_cast<duration_ns>(steady_clock::now().time_since_epoch());
    return getTimestampMsec(time_span.count());
#endif
#endif
  }

  bool RTProfile::isDeviceProfileOn() const {
    // Device profiling is not valid in cpu flow or old emulation flow
    if (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::CPU
       || XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::COSIM_EM)
      return false;

    //return ProfileFlags & RTProfile::PROFILE_DEVICE;
    return ProfileFlags & RTProfile::PROFILE_DEVICE_COUNTERS;
  }

  void RTProfile::setTransferTrace(const std::string traceStr) {
    std::string option = traceStr;
    std::transform(option.begin(), option.end(), option.begin(), ::tolower);

    if (option.find("off") != std::string::npos)         DeviceTraceOption = DEVICE_TRACE_OFF;
    else if (option.find("fine") != std::string::npos)   DeviceTraceOption = DEVICE_TRACE_FINE;
    else if (option.find("coarse") != std::string::npos) DeviceTraceOption = DEVICE_TRACE_COARSE;
    else {
      xrt::message::send(xrt::message::severity_level::WARNING,
        "The data_transfer_trace setting of " + traceStr + " is not recognized. Please use fine|coarse|off.");
    }

    if ((DeviceTraceOption == DEVICE_TRACE_COARSE) && (std::getenv("XCL_EMULATION_MODE"))) {
      xrt::message::send(xrt::message::severity_level::WARNING,
        "The data_transfer_trace setting of " + traceStr + " is not supported in emulation. Fine will be used.");
      DeviceTraceOption = DEVICE_TRACE_FINE;
    }
  }

  void RTProfile::setStallTrace(const std::string traceStr) {
    std::string option = traceStr;
    std::transform(option.begin(), option.end(), option.begin(), ::tolower);

    // Memory = External Axi bus to memory
    // Dataflow = Intra kernel Stream
    // Pipe = Inter Kernel pipes

    if (option.find("off") != std::string::npos)      StallTraceOption = STALL_TRACE_OFF;
    else if (option.find("memory") != std::string::npos) StallTraceOption = STALL_TRACE_EXT;
    else if (option.find("dataflow") != std::string::npos) StallTraceOption = STALL_TRACE_INT;
    else if (option.find("pipe") != std::string::npos) StallTraceOption = STALL_TRACE_STR;
    else if (option.find("all") != std::string::npos) StallTraceOption = STALL_TRACE_ALL;
    else {
      xrt::message::send(xrt::message::severity_level::WARNING,
        "The stall_trace setting of " + traceStr + " is not recognized. Please use memory|dataflow|pipe|all|off.");
    }
  }

  void RTProfile::attach(WriterI* writer)
  {
    std::lock_guard < std::mutex > lock(LogMutex);
    auto itr = std::find(Writers.begin(), Writers.end(), writer);
    if (itr == Writers.end())
      Writers.push_back(writer);
  }

  void RTProfile::detach(WriterI* writer)
  {
    std::lock_guard < std::mutex > lock(LogMutex);
    auto itr = std::find(Writers.begin(), Writers.end(), writer);
    if (itr != Writers.end())
      Writers.erase(itr);
  }

  void RTProfile::commandKindToString(e_profile_command_kind objKind,
      std::string& commandString
  ) const
  {
    switch (objKind) {
    case READ_BUFFER:
      commandString = "READ_BUFFER";
      break;
    case WRITE_BUFFER:
      commandString = "WRITE_BUFFER";
      break;
    case EXECUTE_KERNEL:
      commandString = "KERNEL";
      break;
    case DEVICE_KERNEL_READ:
      commandString = "KERNEL_READ";
      break;
    case DEVICE_KERNEL_WRITE:
      commandString = "KERNEL_WRITE";
      break;
    case DEVICE_KERNEL_EXECUTE:
      commandString = "KERNEL_EXECUTE";
      break;
    case DEVICE_BUFFER_READ:
      commandString = "READ_BUFFER_DEVICE";
      break;
    case DEVICE_BUFFER_WRITE:
      commandString = "WRITE_BUFFER_DEVICE";
      break;
    case DEPENDENCY_EVENT:
      commandString = "DEPENDENCY_EVENT";
      break;
    default:
      assert(0);
      break;
    }
  }

  void RTProfile::commandStageToString(e_profile_command_state objStage,
      std::string& stageString) const
  {
    switch (objStage) {
    case QUEUE:
      stageString = "QUEUE";
      break;
    case SUBMIT:
      stageString = "SUBMIT";
      break;
    case START:
      stageString = "START";
      break;
    case END:
      stageString = "END";
      break;
    case COMPLETE:
      stageString = "COMPLETE";
      break;
    default:
      assert(0);
      break;
    }
  }

  void RTProfile::setTimeStamp(e_profile_command_state objStage,
      TimeTrace* traceObject, double timeStamp)
  {
    switch (objStage) {
    case QUEUE:
      traceObject->Queue= timeStamp;
      break;
    case SUBMIT:
      traceObject->Submit= timeStamp;
      break;
    case START:
      traceObject->Start= timeStamp;
      break;
    case END:
      traceObject->End= timeStamp;
      break;
    case COMPLETE:
      traceObject->Complete= timeStamp;
      break;
    default:
      assert(0);

      break;
    }
  }

  // Get a device timestamp
  double RTProfile::getDeviceTimeStamp(double hostTimeStamp, std::string& deviceName)
  {
    double deviceTimeStamp = hostTimeStamp;

    // In HW emulation, use estimated host timestamp based on device clock cycles (in psec from HAL)
    if (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::HW_EM) {
      size_t dts = XCL::RTSingleton::Instance()->getDeviceTimestamp(deviceName);
      deviceTimeStamp = dts / 1000000.0;
    }

    return deviceTimeStamp;
  }

  void RTProfile::logDataTransfer(uint64_t objId, e_profile_command_kind objKind,
      e_profile_command_state objStage, size_t objSize, uint32_t contextId,
      uint32_t numDevices, std::string deviceName, uint32_t commandQueueId,
      uint64_t address, const std::string& bank, std::thread::id threadId,
      const std::string eventString, const std::string dependString, double timestampMsec)
  {
    double timeStamp = (timestampMsec > 0.0) ? timestampMsec : getTraceTime();
    double deviceTimeStamp = getDeviceTimeStamp(timeStamp, deviceName);
#ifdef USE_DEVICE_TIMELINE
    timeStamp = deviceTimeStamp;
#endif

    std::string commandString;
    std::string stageString;
    std::lock_guard < std::mutex > lock(LogMutex);
    commandKindToString(objKind, commandString);
    commandStageToString(objStage, stageString);

    // Collect time trace
    BufferTrace* traceObject = nullptr;
    auto itr = BufferTraceMap.find(objId);
    if (itr == BufferTraceMap.end()) {
      traceObject = BufferTrace::reuse();
      BufferTraceMap[objId] = traceObject;
    }
    else {
      traceObject = itr->second;
    }
    setTimeStamp(objStage, traceObject, timeStamp);

    if (objStage == END) {
      // Collect performance counters
      switch (objKind) {
      case READ_BUFFER: {
        PerfCounters.logBufferRead(objSize, (traceObject->End - traceObject->Start), contextId, numDevices);
        PerfCounters.pushToSortedTopUsage(traceObject, true);
        break;
      }
      case WRITE_BUFFER: {
        PerfCounters.logBufferWrite(objSize, (traceObject->End - traceObject->Start), contextId, numDevices);
        PerfCounters.pushToSortedTopUsage(traceObject, false);
        break;
      }
      default:
        assert(0);
        break;
      }

      // Mark and keep top trace data
      // Data can be additionally streamed to a data transfer record
      traceObject->Address = address;
      traceObject->Size = objSize;
      traceObject->ContextId = contextId;
      traceObject->CommandQueueId = commandQueueId;
      auto itr = BufferTraceMap.find(objId);
      BufferTraceMap.erase(itr);

      // Store thread IDs into set
      addToThreadIds(threadId);
    }

    writeTimelineTrace(timeStamp, commandString, stageString, eventString, dependString,
                       objSize, address, bank, threadId);

    // Write host event to trace buffer
    if (objStage == START || objStage == END) {
      xclPerfMonEventType eventType = (objStage == START) ? XCL_PERF_MON_START_EVENT : XCL_PERF_MON_END_EVENT;
      xclPerfMonEventID eventID = (objKind == READ_BUFFER) ? XCL_PERF_MON_READ_ID : XCL_PERF_MON_WRITE_ID;
      xdp::profile::platform::write_host_event(XCL::RTSingleton::Instance()->getcl_platform_id(), eventType, eventID);
    }
  }

  //an empty cu_name indicates its doing original "kernel" profiling
  //a non empty call implies we need to collect compute unit based info.
  //Both will be called for a run, since we need to collect/display both
  //  kernel as well as compute unit info.
  void RTProfile::logKernelExecution(uint64_t objId, uint32_t programId, uint64_t eventId, e_profile_command_state objStage,
      std::string kernelName, std::string xclbinName, uint32_t contextId, uint32_t commandQueueId,
      const std::string& deviceName, uid_t uid, const size_t* globalWorkSize, size_t workGroupSize,
      const size_t* localWorkDim, const std::string& cu_name, const std::string eventString,
      const std::string dependString, double timeStampMsec)
  {
    double timeStamp = (timeStampMsec > 0.0) ? timeStampMsec : getTraceTime();
    //if (GetFirstCUTimestamp && !cu_name.empty()) {
    if (GetFirstCUTimestamp && (objStage == START)) {
      DeviceProfile->setStartTimeMsec(timeStamp);
      GetFirstCUTimestamp = false;
    }

    std::lock_guard<std::mutex> lock(LogMutex);

    // TODO: create unique name for device since currently all devices are called fpga0
    // NOTE: see also logCounters for corresponding device name for counters
    std::string newDeviceName = deviceName + "-" + std::to_string(uid);

    // In HW emulation, use estimated host timestamp based on device clock cycles
    double deviceTimeStamp = getDeviceTimeStamp(timeStamp, newDeviceName);

#ifdef USE_DEVICE_TIMELINE
    timeStamp = deviceTimeStamp;
#endif

    // Placeholders for ID and name used in device trace reporting
    // TODO: need to grab actual kernel name and context ID from AXI IDs and metadata
    CurrentContextId = contextId;
    CurrentKernelName = kernelName;
    CurrentDeviceName = newDeviceName;
    CurrentBinaryName = xclbinName;

    std::string commandString;
    std::string stageString;
    commandKindToString(EXECUTE_KERNEL, commandString);
    commandStageToString(objStage, stageString);

    std::string cuName("");
    std::string cuName2("");

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
      if (objStage == START) {
        XOCL_DEBUGF("logKernelExecution: kernel START @ %.3f msec for %s\n", deviceTimeStamp, newKernelName.c_str());
        PerfCounters.logKernelExecutionStart(newKernelName, newDeviceName, deviceTimeStamp);
      }
      else if (objStage == END) {
        XOCL_DEBUGF("logKernelExecution: kernel END @ %.3f msec for %s\n", deviceTimeStamp, newKernelName.c_str());
        PerfCounters.logKernelExecutionEnd(newKernelName, newDeviceName, deviceTimeStamp);
      }

      // Collect trace objects
      KernelTrace* traceObject = nullptr;
      auto itr = KernelTraceMap.find(eventId);
      if(itr == KernelTraceMap.end()) {
        traceObject = KernelTrace::reuse();
        KernelTraceMap[eventId] = traceObject;
      } else {
        traceObject = itr->second;
      }
      setTimeStamp(objStage, traceObject, deviceTimeStamp);
      if (objStage == END) {
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

        auto itr = KernelTraceMap.find(eventId);
        KernelTraceMap.erase(itr);
        // Only log Valid trace objects
        if (traceObject->getStart() > 0.0 && traceObject->getStart() < deviceTimeStamp) {
          PerfCounters.pushToSortedTopUsage(traceObject);
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
      // Naming used in profile summary
      cuName = newDeviceName + "|" + kernelName + "|" + globalSize + "|" + localSize
               + "|" + cu_name + "|" + std::to_string(0x1);
      // Naming used in timeline trace
      cuName2 = kernelName + "|" + localSize + "|" + cu_name;
      if (objStage == START) {
        XOCL_DEBUGF("logKernelExecution: CU START @ %.3f msec for %s\n", deviceTimeStamp, cuName.c_str());
        if (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::CPU) {
          PerfCounters.logComputeUnitExecutionStart(cuName, deviceTimeStamp);
          PerfCounters.logComputeUnitDeviceStart(newDeviceName, timeStamp);
        }
      }
      else if (objStage == END) {
        XOCL_DEBUGF("logKernelExecution: CU END @ %.3f msec for %s\n", deviceTimeStamp, cuName.c_str());
        // This is updated through HAL
        if (XCL::RTSingleton::Instance()->getFlowMode() != XCL::RTSingleton::CPU)
          deviceTimeStamp = 0;
        PerfCounters.logComputeUnitExecutionEnd(cuName, deviceTimeStamp);
      }

      // Store mapping of CU name to kernel name
      ComputeUnitKernelNameMap[cu_name] = kernelName;

      //New timeline summary data.
      std::string uniqueCUName("KERNEL|");
      (uniqueCUName += newDeviceName) += "|";
      (uniqueCUName += xclbinName) += "|";
      (uniqueCUName += cuName2) += "|";
      commandString = uniqueCUName + std::to_string(workGroupSize);
      ComputeUnitKernelTraceMap [cu_name] = commandString;

      if (XCL::RTSingleton::Instance()->getFlowMode() == XCL::RTSingleton::CPU)
        writeTimelineTrace(timeStamp, uniqueCUName, stageString, eventString, dependString,
                            objId, workGroupSize);
    }

    // Write host event to trace buffer (only if used)
    if (objStage == START || objStage == END) {
      xclPerfMonEventType eventType = (objStage == START) ? XCL_PERF_MON_START_EVENT : XCL_PERF_MON_END_EVENT;
      xclPerfMonEventID eventID = (cu_name.empty()) ? XCL_PERF_MON_KERNEL0_ID : XCL_PERF_MON_CU0_ID;
      xdp::profile::platform::write_host_event(XCL::RTSingleton::Instance()->getcl_platform_id(), eventType, eventID);
    }
  }

  void RTProfile::logDependency(e_profile_command_kind objKind,
      const std::string eventString, const std::string dependString)
  {
    std::string commandString;
    std::lock_guard < std::mutex > lock(LogMutex);
    commandKindToString(objKind, commandString);

    writeTimelineTrace(getTraceTime(), commandString, "", eventString, dependString);
  }

  xclPerfMonEventID
  RTProfile::getFunctionEventID(const std::string &functionName, long long queueAddress)
  {
    // Ignore 'release' functions
    if (functionName.find("Release") != std::string::npos)
      return XCL_PERF_MON_IGNORE_EVENT;

#if 0
    if (queueAddress == 0)
      return XCL_PERF_MON_GENERAL_ID;
    else
      return XCL_PERF_MON_QUEUE_ID;
#endif

    // Get function-specific ID
    // NOTE: similar to list in convertApiState() in tools/sda2wdb/wdbWriter.cxx
    if (functionName.find("clGetPlatformIDs") != std::string::npos)
      return XCL_PERF_MON_API_GET_PLATFORM_ID;
    else if (functionName.find("clGetPlatformInfo") != std::string::npos)
      return XCL_PERF_MON_API_GET_PLATFORM_INFO_ID;
    else if (functionName.find("clGetDeviceIDs") != std::string::npos)
      return XCL_PERF_MON_API_GET_DEVICE_ID;
    else if (functionName.find("clGetDeviceInfo") != std::string::npos)
      return XCL_PERF_MON_API_GET_DEVICE_INFO_ID;
    else if (functionName.find("clBuildProgram") != std::string::npos)
      return XCL_PERF_MON_API_BUILD_PROGRAM_ID;
    else if (functionName.find("clCreateContextFromType") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_CONTEXT_TYPE_ID;
    else if (functionName.find("clCreateContext") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_CONTEXT_ID;
    else if (functionName.find("clCreateCommandQueue") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_COMMAND_QUEUE_ID;
    else if (functionName.find("clCreateProgramWithBinary") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_PROGRAM_BINARY_ID;
    else if (functionName.find("clCreateBuffer") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_BUFFER_ID;
    else if (functionName.find("clCreateImage") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_IMAGE_ID;
    else if (functionName.find("clCreateKernel") != std::string::npos)
      return XCL_PERF_MON_API_CREATE_KERNEL_ID;
    else if (functionName.find("clSetKernelArg") != std::string::npos)
      return XCL_PERF_MON_API_KERNEL_ARG_ID;
    else if (functionName.find("clWaitForEvents") != std::string::npos)
      return XCL_PERF_MON_API_WAIT_FOR_EVENTS_ID;
    else if (functionName.find("clEnqueueReadBuffer") != std::string::npos)
      return XCL_PERF_MON_API_READ_BUFFER_ID;
    else if (functionName.find("clEnqueueWriteBuffer") != std::string::npos)
      return XCL_PERF_MON_API_WRITE_BUFFER_ID;
    else if (functionName.find("clEnqueueReadImage") != std::string::npos)
      return XCL_PERF_MON_API_READ_IMAGE_ID;
    else if (functionName.find("clEnqueueWriteImage") != std::string::npos)
      return XCL_PERF_MON_API_WRITE_IMAGE_ID;
else if (functionName.find("clEnqueueMigrateMemObjects") != std::string::npos)
      return XCL_PERF_MON_API_MIGRATE_MEM_OBJECTS_ID;
    else if (functionName.find("clEnqueueMigrateMem") != std::string::npos)
      return XCL_PERF_MON_API_MIGRATE_MEM_ID;
    else if (functionName.find("clEnqueueMapBuffer") != std::string::npos)
      return XCL_PERF_MON_API_MAP_BUFFER_ID;
    else if (functionName.find("clEnqueueUnmapMemObject") != std::string::npos)
      return XCL_PERF_MON_API_UNMAP_MEM_OBJECT_ID;
    else if (functionName.find("clEnqueueNDRangeKernel") != std::string::npos)
      return XCL_PERF_MON_API_NDRANGE_KERNEL_ID;
    else if (functionName.find("clEnqueueTask") != std::string::npos)
      return XCL_PERF_MON_API_TASK_ID;

    // Function not in reported list so ignore
    return XCL_PERF_MON_IGNORE_EVENT;
  }

  void RTProfile::logFunctionCallStart(const char* functionName, long long queueAddress)
  {
#ifdef USE_DEVICE_TIMELINE
    double timeStamp = getDeviceTimeStamp(getTraceTime(), CurrentDeviceName);
#else
    double timeStamp = getTraceTime();
#endif

    std::string name(functionName);
    if (name.find("MigrateMem") != std::string::npos)
      MigrateMemCalls++;

    if (queueAddress == 0)
      name += "|General";
    else
      (name += "|") +=std::to_string(queueAddress);
    std::lock_guard<std::mutex> lock(LogMutex);
    PerfCounters.logFunctionCallStart(functionName, timeStamp);
    writeTimelineTrace(timeStamp, name.c_str(), "START");
    FunctionStartLogged = true;

    // Write host event to trace buffer
    xclPerfMonEventID eventID = getFunctionEventID(name, queueAddress);
    if (eventID != XCL_PERF_MON_IGNORE_EVENT) {
      xclPerfMonEventType eventType = XCL_PERF_MON_START_EVENT;
      xdp::profile::platform::write_host_event(XCL::RTSingleton::Instance()->getcl_platform_id(), eventType, eventID);
    }
  }

  void RTProfile::logFunctionCallEnd(const char* functionName, long long queueAddress)
  {
    // Log function call start if not done so already
    // NOTE: this addresses a race condition when constructing the singleton (CR 963297)
    if (!FunctionStartLogged)
      logFunctionCallStart(functionName, queueAddress);

#ifdef USE_DEVICE_TIMELINE
    double timeStamp = getDeviceTimeStamp(getTraceTime(), CurrentDeviceName);
#else
    double timeStamp = getTraceTime();
#endif

    std::string name(functionName);
    if (queueAddress == 0)
      name += "|General";
    else
      (name += "|") +=std::to_string(queueAddress);

    std::lock_guard<std::mutex> lock(LogMutex);
    PerfCounters.logFunctionCallEnd(functionName, timeStamp);
    writeTimelineTrace(timeStamp, name.c_str(), "END");

    // Write host event to trace buffer
    xclPerfMonEventID eventID = getFunctionEventID(name, queueAddress);
    if (eventID != XCL_PERF_MON_IGNORE_EVENT) {
      xclPerfMonEventType eventType = XCL_PERF_MON_END_EVENT;
      xdp::profile::platform::write_host_event(XCL::RTSingleton::Instance()->getcl_platform_id(), eventType, eventID);
    }
  }

  // Write API call events to trace
  void RTProfile::writeTimelineTrace( double traceTime,
      const char* functionName, const char* eventName) const
  {
    if(!this->isTimelineTraceFileOn())
      return;

    for(auto w : Writers) {
      w->writeTimeline(traceTime, functionName, eventName);
    }
  }

  // Write kernel event to trace
  void RTProfile::writeTimelineTrace( double traceTime,
      const std::string& commandString, const std::string& stageString,
      const std::string& eventString, const std::string& dependString,
      uint64_t objId, size_t size) const
  {
    if(!this->isTimelineTraceFileOn())
      return;

    for (auto w : Writers) {
      w->writeTimeline(traceTime, commandString, stageString, eventString,
                       dependString, objId, size);
    }
  }

  // Write data transfer event to trace
  void RTProfile::writeTimelineTrace(double traceTime,
      const std::string& commandString, const std::string& stageString,
      const std::string& eventString, const std::string& dependString,
      size_t size, uint64_t address, const std::string& bank,
      std::thread::id threadId) const
  {
    if(!this->isTimelineTraceFileOn())
      return;

    for (auto w : Writers) {
      w->writeTimeline(traceTime, commandString, stageString, eventString, dependString,
                       size, address, bank, threadId);
    }
  }

  // Write Dependency information
  void RTProfile::writeTimelineTrace(double traceTime,
      const std::string& commandString, const std::string& stageString,
      const std::string& eventString, const std::string& dependString) const
  {
    if(!this->isTimelineTraceFileOn())
      return;

    for (auto w : Writers) {
      w->writeTimeline(traceTime, commandString, stageString, eventString, dependString);
    }
  }

  // Set kernel clock freq on device
  void RTProfile::setKernelClockFreqMHz(const std::string &deviceName, unsigned int kernelClockRateMHz) {
    if (DeviceProfile == NULL)
      return;

    DeviceProfile->setKernelClockFreqMHz(deviceName, kernelClockRateMHz);
  }

  // Get kernel clock freq on device
  unsigned int RTProfile::getKernelClockFreqMHz(std::string &deviceName) {
	if (DeviceProfile == NULL)
	  return 300;

    return DeviceProfile->getKernelClockFreqMHz(deviceName);
  }

  // Set device clock freq
  void RTProfile::setDeviceClockFreqMHz(double deviceClockRateMHz) {
    if (DeviceProfile == NULL)
      return;

    DeviceProfile->setDeviceClockFreqMHz(deviceClockRateMHz);

    PerfCounters.setAllDeviceClockFreqMhz(deviceClockRateMHz);
    PerfCounters.setAllDeviceBufferBitWidth( DeviceProfile->getGlobalMemoryBitWidth() );
    PerfCounters.setAllDeviceKernelBitWidth( DeviceProfile->getGlobalMemoryBitWidth() );
  }

  // Set device trace clock freq
  void RTProfile::setDeviceTraceClockFreqMHz(double deviceTraceClockRateMHz) {
    if (DeviceProfile == NULL)
      return;

    DeviceProfile->setTraceClockFreqMHz(deviceTraceClockRateMHz);
  }

  // Set global memory bit width
  void RTProfile::setGlobalMemoryBitWidth(uint32_t bitWidth) {
    if (DeviceProfile == NULL)
      return;

    DeviceProfile->setGlobalMemoryBitWidth(bitWidth);
  }

  // Get global memory bit width
  uint32_t RTProfile::getGlobalMemoryBitWidth() {
    if (DeviceProfile == NULL)
      return XPAR_AXI_PERF_MON_0_SLOT0_DATA_WIDTH;

    return DeviceProfile->getGlobalMemoryBitWidth();
  }

  // Get threshold of device trace fifo where we decide to read it
  uint32_t RTProfile::getTraceSamplesThreshold() {
    if (DeviceProfile == NULL)
      return 1000;

    return DeviceProfile->getTraceSamplesThreshold();
  }

  // Get sample interval for reading device profile counters
  uint32_t RTProfile::getSampleIntervalMsec() {
    if (DeviceProfile == NULL)
      return 10;

    return DeviceProfile->getSampleIntervalMsec();
  }

  // Log device trace results
  void RTProfile::logDeviceTrace(std::string deviceName, std::string binaryName,
      xclPerfMonType type, xclTraceResultsVector& traceVector) {
    if (DeviceProfile == NULL || traceVector.mLength == 0)
      return;

    std::lock_guard<std::mutex> lock(LogMutex);
    RTProfileDevice::TraceResultVector resultVector;
    DeviceProfile->logTrace(deviceName, type, traceVector, resultVector);

    if (resultVector.empty())
      return;

    // Log for summary purposes
    uint64_t index = 0;
    for (auto it = resultVector.begin(); it != resultVector.end(); it++) {
      DeviceTrace* tr = DeviceTrace::reuse();

      // Copy trace results
      // TODO: replace with actual device and kernel names (interpreted from AXI IDs)
      tr->DeviceName = deviceName;
      tr->Name = CurrentKernelName;
      tr->ContextId = CurrentContextId;
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
      PerfCounters.logDeviceEvent(tr->DeviceName, tr->Name, tr->NumBytes, durationMsec,
          DeviceProfile->getGlobalMemoryBitWidth(), DeviceProfile->getGlobalMemoryClockFreqMHz(),
          isKernel, isRead, isKernelTransfer);
      PerfCounters.pushToSortedTopUsage(tr, isRead, isKernelTransfer);
    }

    // Write trace results vector to files
    if (this->isTimelineTraceFileOn()) {
      for (auto w : Writers) {
        w->writeDeviceTrace(resultVector, deviceName, binaryName);
      }
    }

    resultVector.clear();
  }

  uint32_t RTProfile::getCounterValue(xclPerfMonCounterType type, uint32_t slotnum,
      xclCounterResults& results) const
  {
    uint32_t counterValue = 0;

    switch (type) {
    case XCL_PERF_MON_WRITE_BYTES:
      counterValue = results.WriteBytes[slotnum];
      break;
    case XCL_PERF_MON_WRITE_TRANX:
      counterValue = results.WriteTranx[slotnum];
      break;
    case XCL_PERF_MON_WRITE_LATENCY:
      counterValue = results.WriteLatency[slotnum];
      break;
    case XCL_PERF_MON_READ_BYTES:
      counterValue = results.ReadBytes[slotnum];
      break;
    case XCL_PERF_MON_READ_TRANX:
      counterValue = results.ReadTranx[slotnum];
      break;
    case XCL_PERF_MON_READ_LATENCY:
      counterValue = results.ReadLatency[slotnum];
      break;
    default:
      break;
    }

#if 0
    // Add counters together from all OCL region masters
    if (numSlots > 2) {
      for (int i=2; i < numSlots; i++) {
        switch (type) {
        case XCL_PERF_MON_WRITE_BYTES:
          counterValue += results.WriteBytes[i];
          break;
        case XCL_PERF_MON_WRITE_TRANX:
          counterValue += results.WriteTranx[i];
          break;
        case XCL_PERF_MON_WRITE_LATENCY:
          counterValue += results.WriteLatency[i];
          break;
        case XCL_PERF_MON_READ_BYTES:
          counterValue += results.ReadBytes[i];
          break;
        case XCL_PERF_MON_READ_TRANX:
          counterValue += results.ReadTranx[i];
          break;
        case XCL_PERF_MON_READ_LATENCY:
          counterValue += results.ReadLatency[i];
          break;
        default:
          break;
        }
      }
    }
#endif

    return counterValue;
  }

  // Log device counters results
  void RTProfile::logDeviceCounters(std::string deviceName, std::string binaryName, xclPerfMonType type,
      xclCounterResults& counterResults, uint64_t timeNsec, bool firstReadAfterProgram)
  {
    // Number of monitor slots
    uint32_t numSlots = 0;
    std::string key = deviceName + "|" + binaryName;
    std::string slotName = "";

#if 0
    if (!firstReadAfterProgram
        && counterResults.WriteBytes[OclSlotIndex] == 0
        && counterResults.ReadBytes[OclSlotIndex] == 0
        && counterResults.WriteBytes[HostSlotIndex] == 0
        && counterResults.ReadBytes[HostSlotIndex] == 0)
      return;

    XOCL_LOG("logDeviceCounters: first read = %d, device: %s\n", firstReadAfterProgram, deviceName.c_str());
    XOCL_LOG("First CU port: write bytes = %u, read bytes = %u\n", counterResults.WriteBytes[OclSlotIndex],
            counterResults.ReadBytes[OclSlotIndex]);
    XOCL_LOG("         Host: write bytes = %u, read bytes = %u\n", counterResults.WriteBytes[HostSlotIndex],
        counterResults.ReadBytes[HostSlotIndex]);
#else
    XOCL_LOG("logDeviceCounters: first read = %d, device: %s\n", firstReadAfterProgram, deviceName.c_str());
#endif

    // If not already defined, zero out rollover values for this device
    if (FinalCounterResultsMap.find(key) == FinalCounterResultsMap.end()) {
      FinalCounterResultsMap[key] = counterResults;

      xclCounterResults rolloverResults;
      memset(&rolloverResults, 0, sizeof(xclCounterResults));
      //rolloverResults.NumSlots = counterResults.NumSlots;
      RolloverCounterResultsMap[key] = rolloverResults;
      RolloverCountsMap[key] = rolloverResults;
    }
    else {
      /*
       * Log SPM Counters
       */
      numSlots = XCL::RTSingleton::Instance()->getProfileNumberSlots(XCL_PERF_MON_MEMORY, deviceName);
      // Traverse all monitor slots (host and all CU ports)
   	   bool deviceDataExists = (DeviceBinaryDataSlotsMap.find(key) == DeviceBinaryDataSlotsMap.end()) ? false : true;
       for (int s=0; s < numSlots; ++s) {
        XCL::RTSingleton::Instance()->getProfileSlotName(XCL_PERF_MON_MEMORY, deviceName, s, slotName);
        if (!deviceDataExists)
          DeviceBinaryDataSlotsMap[key].push_back(slotName);
        uint32_t prevWriteBytes   = FinalCounterResultsMap[key].WriteBytes[s];
        uint32_t prevReadBytes    = FinalCounterResultsMap[key].ReadBytes[s];
        uint32_t prevWriteTranx   = FinalCounterResultsMap[key].WriteTranx[s];
        uint32_t prevReadTranx    = FinalCounterResultsMap[key].ReadTranx[s];
        uint32_t prevWriteLatency = FinalCounterResultsMap[key].WriteLatency[s];
        uint32_t prevReadLatency  = FinalCounterResultsMap[key].ReadLatency[s];

        // Check for rollover of byte counters; if detected, add 2^32
        // Otherwise, if first read after program with binary, then capture bytes from previous xclbin
        if (!firstReadAfterProgram) {
          if (counterResults.WriteBytes[s] < prevWriteBytes)
            RolloverCountsMap[key].WriteBytes[s]    += 1;
          if (counterResults.ReadBytes[s] < prevReadBytes)
            RolloverCountsMap[key].ReadBytes[s]     += 1;
          if (counterResults.WriteTranx[s] < prevWriteTranx)
            RolloverCountsMap[key].WriteTranx[s]    += 1;
          if (counterResults.ReadTranx[s] < prevReadTranx)
            RolloverCountsMap[key].ReadTranx[s]     += 1;
          if (counterResults.WriteLatency[s] < prevWriteLatency)
            RolloverCountsMap[key].WriteLatency[s]  += 1;
          if (counterResults.ReadLatency[s] < prevReadLatency)
            RolloverCountsMap[key].ReadLatency[s]   += 1;
        }
        else {
          RolloverCounterResultsMap[key].WriteBytes[s]    += prevWriteBytes;
          RolloverCounterResultsMap[key].ReadBytes[s]     += prevReadBytes;
          RolloverCounterResultsMap[key].WriteTranx[s]    += prevWriteTranx;
          RolloverCounterResultsMap[key].ReadTranx[s]     += prevReadTranx;
          RolloverCounterResultsMap[key].WriteLatency[s]  += prevWriteLatency;
          RolloverCounterResultsMap[key].ReadLatency[s]   += prevReadLatency;
        }
   	  }
      /*
       * Log SAM Counters
       */     
      numSlots = XCL::RTSingleton::Instance()->getProfileNumberSlots(XCL_PERF_MON_ACCEL, deviceName);
      for (int s=0; s < numSlots; ++s) {
        uint32_t prevCuExecCount      = FinalCounterResultsMap[key].CuExecCount[s];
        uint32_t prevCuExecCycles     = FinalCounterResultsMap[key].CuExecCycles[s];
        uint32_t prevCuStallExtCycles = FinalCounterResultsMap[key].CuStallExtCycles[s];
        uint32_t prevCuStallIntCycles = FinalCounterResultsMap[key].CuStallIntCycles[s];
        uint32_t prevCuStallStrCycles = FinalCounterResultsMap[key].CuStallStrCycles[s];
        if (!firstReadAfterProgram) {
          if (counterResults.CuExecCycles[s] < prevCuExecCycles)
            RolloverCountsMap[key].CuExecCycles[s]     += 1;
          if (counterResults.CuStallExtCycles[s] < prevCuStallExtCycles)
            RolloverCountsMap[key].CuStallExtCycles[s] += 1;
          if (counterResults.CuStallIntCycles[s] < prevCuStallIntCycles)
            RolloverCountsMap[key].CuStallIntCycles[s] += 1;
          if (counterResults.CuStallStrCycles[s] < prevCuStallStrCycles)
            RolloverCountsMap[key].CuStallStrCycles[s] += 1;
        }
        else {
          RolloverCounterResultsMap[key].CuExecCount[s]      += prevCuExecCount;
          RolloverCounterResultsMap[key].CuExecCycles[s]     += prevCuExecCycles;
          RolloverCounterResultsMap[key].CuStallExtCycles[s] += prevCuStallExtCycles;
          RolloverCounterResultsMap[key].CuStallIntCycles[s] += prevCuStallIntCycles;
          RolloverCounterResultsMap[key].CuStallStrCycles[s] += prevCuStallStrCycles;
        }
      }
      FinalCounterResultsMap[key] = counterResults;
    }
    /*
     * Update Stats Database
     */
    uint32_t kernelClockMhz = getKernelClockFreqMHz(deviceName);
    double deviceCyclesMsec = kernelClockMhz * 1000.0 ;
    std::string cuName = "";
    std::string kernelName ="";
    bool deviceDataExists = (DeviceBinaryCuSlotsMap.find(key) == DeviceBinaryCuSlotsMap.end()) ? false : true;
    xclCounterResults rolloverResults = RolloverCounterResultsMap.at(key);
    xclCounterResults rolloverCounts = RolloverCountsMap.at(key);
    for (int s=0; s < numSlots; ++s) {
      XCL::RTSingleton::Instance()->getProfileSlotName(XCL_PERF_MON_ACCEL, deviceName, s, cuName);
      XCL::RTSingleton::Instance()->getProfileKernelName(deviceName, cuName, kernelName);
      if (!deviceDataExists)
        DeviceBinaryCuSlotsMap[key].push_back(cuName);
      uint32_t cuExecCount = counterResults.CuExecCount[s] + rolloverResults.CuExecCount[s];
      uint64_t cuExecCycles = counterResults.CuExecCycles[s] + rolloverResults.CuExecCycles[s]
                                + (rolloverCounts.CuExecCycles[s] * 4294967296UL);
      uint32_t cuMaxExecCycles  = counterResults.CuMaxExecCycles[s];
      uint32_t cuMinExecCycles  = counterResults.CuMinExecCycles[s];
      double cuRunTimeMsec = (double) cuExecCycles / deviceCyclesMsec;
      double cuMaxExecCyclesMsec = (double) cuMaxExecCycles / deviceCyclesMsec;
      double cuMinExecCyclesMsec = (double) cuMinExecCycles / deviceCyclesMsec;
      //XOCL_DEBUGF("[RT_PROFILE] cuName : %s exec cycles : %d runtime %f \n", cuName.c_str(), cuExecCycles, cuRunTimeMsec);
      PerfCounters.logComputeUnitStats(cuName, kernelName, cuRunTimeMsec, cuMaxExecCyclesMsec, 
                                        cuMinExecCyclesMsec, cuExecCount, kernelClockMhz);
    }
#ifdef XDP_VERBOSE
    if (this->isTimelineTraceFileOn()) {
      static uint32_t sampleNum = 0;
#ifdef USE_DEVICE_TIMELINE
      double timeStamp = getDeviceTimeStamp(getTimestampMsec(timeNsec), deviceName);
#else
      double timeStamp = getTimestampMsec(timeNsec);
#endif
      std::lock_guard < std::mutex > lock(LogMutex);
      for (auto w : Writers) {
        w->writeDeviceCounters(type, counterResults, timeStamp, sampleNum, firstReadAfterProgram);
      }
      sampleNum++;
    }
#endif
  }

  void RTProfile::writeAPISummary(WriterI* writer) const
  {
    PerfCounters.writeAPISummary(writer);
  }


  void RTProfile::writeKernelSummary(WriterI* writer) const
  {
    PerfCounters.writeKernelSummary(writer);
  }

  void RTProfile::writeComputeUnitSummary(WriterI* writer) const
  {
    PerfCounters.writeComputeUnitSummary(writer);
  }

  void RTProfile::writeAcceleratorSummary(WriterI* writer) const
  {
    PerfCounters.writeAcceleratorSummary(writer);
  }

  void RTProfile::writeHostTransferSummary(WriterI* writer) const
  {
    uint64_t totalReadBytes    = 0;
    uint64_t totalWriteBytes   = 0;
    uint64_t totalReadLatency  = 0;
    uint64_t totalWriteLatency = 0;
    double totalReadTimeMsec   = 0.0;
    double totalWriteTimeMsec  = 0.0;

    // Get total bytes and total time (currently derived from latency)
    // across all devices
    //
    // CR 951564: Use APM counters to calculate throughput (i.e., byte count and total time)
    // NOTE: for now, we only use this for writes (see PerformanceCounter::writeHostTransferSummary)
    auto iter = FinalCounterResultsMap.begin();
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName))
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));
      uint32_t  numHostSlots = XCL::RTSingleton::Instance()->getProfileNumberSlots(XCL_PERF_MON_HOST, deviceName);
      for (uint32_t s=HostSlotIndex; s < HostSlotIndex + numHostSlots; s++) {
        totalReadBytes += counterResults.ReadBytes[s]
                          + (rolloverCounts.ReadBytes[s] * 4294967296UL);
        totalWriteBytes += counterResults.WriteBytes[s]
                          + (rolloverCounts.WriteBytes[s] * 4294967296UL);
        // Total transfer time = sum of all tranx latencies
        // msec = cycles / (1000 * (Mcycles/sec))
        totalReadLatency += counterResults.ReadLatency[s]
                            + (rolloverCounts.ReadLatency[s] * 4294967296UL);
        totalWriteLatency += counterResults.WriteLatency[s]
                            + (rolloverCounts.WriteLatency[s] * 4294967296UL);
      }
    }
    totalReadTimeMsec = totalReadLatency / (1000.0 * DeviceProfile->getDeviceClockFreqMHz());
    totalWriteTimeMsec = totalWriteLatency / (1000.0 * DeviceProfile->getDeviceClockFreqMHz());

    // Get maximum throughput rates
    double readMaxBandwidthMBps = 0.0;
    double writeMaxBandwidthMBps = 0.0;
    if (XCL::RTSingleton::Instance()->getFlowMode() != XCL::RTSingleton::CPU
        && XCL::RTSingleton::Instance()->getFlowMode() != XCL::RTSingleton::COSIM_EM) {
      readMaxBandwidthMBps = XCL::RTSingleton::Instance()->getReadMaxBandwidthMBps();
      writeMaxBandwidthMBps = XCL::RTSingleton::Instance()->getWriteMaxBandwidthMBps();
    }

    PerfCounters.writeHostTransferSummary(writer, true,  totalReadBytes,  totalReadTimeMsec,  readMaxBandwidthMBps);
    PerfCounters.writeHostTransferSummary(writer, false, totalWriteBytes, totalWriteTimeMsec, writeMaxBandwidthMBps);
  }

  // Max. achievable bandwidth between kernels and DDR global memory = 60% of 10.7 GBps for PCIe Gen 3
  // TODO: this should come from benchmarking results
  double RTProfile::getGlobalMemoryMaxBandwidthMBps() const
  {
    double maxBandwidthMBps =
        0.6 * (DeviceProfile->getGlobalMemoryBitWidth() / 8) * DeviceProfile->getGlobalMemoryClockFreqMHz();
    return maxBandwidthMBps;
  }

  void RTProfile::writeStallSummary(WriterI* writer) const
  {
    auto iter = FinalCounterResultsMap.begin();
    double deviceCyclesMsec = (DeviceProfile->getDeviceClockFreqMHz() * 1000.0);
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName) || (DeviceBinaryCuSlotsMap.find(key) == DeviceBinaryCuSlotsMap.end()))
        continue;

    // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (RolloverCounterResultsMap.find(key) != RolloverCounterResultsMap.end())
        rolloverResults = RolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      std::string cuName = "";

      uint32_t numSlots = DeviceBinaryCuSlotsMap.at(key).size();
      for (int s=0; s < numSlots; ++s) {
        cuName = DeviceBinaryCuSlotsMap.at(key)[s];
        uint32_t cuExecCount = counterResults.CuExecCount[s] + rolloverResults.CuExecCount[s];
        uint64_t cuExecCycles = counterResults.CuExecCycles[s] + rolloverResults.CuExecCycles[s]
                                  + (rolloverCounts.CuExecCycles[s] * 4294967296UL);
        uint64_t cuStallExtCycles = counterResults.CuStallExtCycles[s] + rolloverResults.CuStallExtCycles[s]
                                  + (rolloverCounts.CuStallExtCycles[s] * 4294967296UL);
        uint64_t cuStallStrCycles = counterResults.CuStallStrCycles[s] + rolloverResults.CuStallStrCycles[s]
                                  + (rolloverCounts.CuStallStrCycles[s] * 4294967296UL);
        uint64_t cuStallIntCycles = counterResults.CuStallIntCycles[s] + rolloverResults.CuStallIntCycles[s]
                                  + (rolloverCounts.CuStallIntCycles[s] * 4294967296UL);
        double cuRunTimeMsec = (double) cuExecCycles / deviceCyclesMsec;
        double cuStallExt =    (double) cuStallExtCycles / deviceCyclesMsec;
        double cuStallStr =    (double) cuStallStrCycles / deviceCyclesMsec;
        double cuStallInt =    (double) cuStallIntCycles / deviceCyclesMsec;
        writer->writeStallSummary(cuName, cuExecCount, cuRunTimeMsec,
                                  cuStallExt, cuStallStr, cuStallInt);
      }
    }
  }

  void RTProfile::writeKernelTransferSummary(WriterI* writer) const
  {
    auto iter = FinalCounterResultsMap.begin();
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName) || (DeviceBinaryDataSlotsMap.find(key) == DeviceBinaryDataSlotsMap.end()))
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (RolloverCounterResultsMap.find(key) != RolloverCounterResultsMap.end())
        rolloverResults = RolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      // Number of monitor slots
      uint32_t numSlots = DeviceBinaryDataSlotsMap.at(key).size();
      uint32_t numHostSlots = XCL::RTSingleton::Instance()->getProfileNumberSlots(XCL_PERF_MON_HOST, deviceName);

      // Total kernel time = sum of all kernel executions
      //double totalKernelTimeMsec = PerfCounters.getTotalKernelExecutionTime(deviceName);
      double maxTransferRateMBps = getGlobalMemoryMaxBandwidthMBps();

      int s = 0;
      if (HostSlotIndex == 0)
        s = numHostSlots;
      for (; s < numSlots; ++s) {
        if (s == HostSlotIndex)
          continue;

   	    std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
        std::string cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
        std::string portName = cuPortName.substr(cuPortName.find_first_of("/")+1);
        std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);

        uint32_t ddrBank;
        std::string argNames;
        getArgumentsBank(deviceName, cuName, portName, argNames, ddrBank);

        double totalCUTimeMsec = PerfCounters.getComputeUnitTotalTime(deviceName, cuName);

        uint64_t totalReadBytes    = counterResults.ReadBytes[s] + rolloverResults.ReadBytes[s]
                                     + (rolloverCounts.ReadBytes[s] * 4294967296UL);
        uint64_t totalWriteBytes   = counterResults.WriteBytes[s] + rolloverResults.WriteBytes[s]
                                     + (rolloverCounts.WriteBytes[s] * 4294967296UL);
        uint64_t totalReadTranx    = counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                                     + (rolloverCounts.ReadTranx[s] * 4294967296UL);
        uint64_t totalWriteTranx   = counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                                     + (rolloverCounts.WriteTranx[s] * 4294967296UL);

        // Total transfer time = sum of all tranx latencies
        // msec = cycles / (1000 * (Mcycles/sec))
        uint64_t totalReadLatency  = counterResults.ReadLatency[s] + rolloverResults.ReadLatency[s]
                                     + (rolloverCounts.ReadLatency[s] * 4294967296UL);
        double totalReadTimeMsec   = totalReadLatency / (1000.0 * DeviceProfile->getDeviceClockFreqMHz());
        uint64_t totalWriteLatency = counterResults.WriteLatency[s] + rolloverResults.WriteLatency[s]
                                     + (rolloverCounts.WriteLatency[s] * 4294967296UL);
        double totalWriteTimeMsec  = totalWriteLatency / (1000.0 * DeviceProfile->getDeviceClockFreqMHz());

        XOCL_DEBUGF("writeKernelTransferSummary: s=%d, reads=%d, writes=%d, %s time = %f msec\n",
            s, totalReadTranx, totalWriteTranx, cuName.c_str(), totalCUTimeMsec);

        // First do READ, then WRITE
        if (totalReadTranx > 0) {
          PerfCounters.writeKernelTransferSummary(writer, deviceName, cuPortName, argNames, ddrBank,
        	  true,  totalReadBytes, totalReadTranx, totalCUTimeMsec, totalReadTimeMsec, maxTransferRateMBps);
        }
        if (totalWriteTranx > 0) {
          PerfCounters.writeKernelTransferSummary(writer, deviceName, cuPortName, argNames, ddrBank,
        	  false, totalWriteBytes, totalWriteTranx, totalCUTimeMsec, totalWriteTimeMsec, maxTransferRateMBps);
        }
      }
    }
  }

  void RTProfile::writeTopKernelSummary(WriterI* writer) const
  {
    PerfCounters.writeTopKernelSummary(writer);
  }

  void RTProfile::writeTopHardwareSummary(WriterI* writer) const
  {
    PerfCounters.writeTopHardwareSummary(writer);
  }

  void RTProfile::writeTopKernelTransferSummary(WriterI* writer) const
  {
    // Iterate over all devices
    auto iter = FinalCounterResultsMap.begin();
    for (; iter != FinalCounterResultsMap.end(); ++iter) {
      std::string key = iter->first;
      std::string deviceName = key.substr(0, key.find_first_of("|"));
      if (!isDeviceActive(deviceName) || (DeviceBinaryDataSlotsMap.find(key) == DeviceBinaryDataSlotsMap.end()))
        continue;

      // Get results
      xclCounterResults counterResults = iter->second;

      xclCounterResults rolloverResults;
      if (RolloverCounterResultsMap.find(key) != RolloverCounterResultsMap.end())
        rolloverResults = RolloverCounterResultsMap.at(key);
      else
        memset(&rolloverResults, 0, sizeof(xclCounterResults));

      xclCounterResults rolloverCounts;
      if (RolloverCountsMap.find(key) != RolloverCountsMap.end())
        rolloverCounts = RolloverCountsMap.at(key);
      else
        memset(&rolloverCounts, 0, sizeof(xclCounterResults));

      // Number of monitor slots
      uint32_t numSlots = DeviceBinaryDataSlotsMap.at(key).size();
      uint32_t numHostSlots = XCL::RTSingleton::Instance()->getProfileNumberSlots(XCL_PERF_MON_HOST, deviceName);

      double maxTransferRateMBps = getGlobalMemoryMaxBandwidthMBps();

      //double totalReadTimeMsec  = PerfCounters.getTotalKernelExecutionTime(deviceName);
      //double totalWriteTimeMsec = totalReadTimeMsec;

      // Maximum bytes per AXI data transfer
      // NOTE: this assumes the entire global memory bit width with a burst of 256 (max burst length of AXI4)
      //       AXI standard also limits a transfer to 4K total bytes
      uint32_t maxBytesPerTransfer = (DeviceProfile->getGlobalMemoryBitWidth() / 8) * 256;
      if (maxBytesPerTransfer > 4096)
        maxBytesPerTransfer = 4096;

      // Gather unique names of monitored CUs on this device
      std::map<std::string, uint64_t> cuNameTranxMap;
      int s;
      if (HostSlotIndex == 0)
        s = numHostSlots;
      else 
        s = 0;
      for (; s < numSlots; ++s) {
        if (s == HostSlotIndex)
          continue;

        std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
        std::string cuName = cuPortName.substr(0, cuPortName.find_first_of("/"));
        cuNameTranxMap[cuName] = 0;
      }

      // Get their total tranx counts
      auto cuIter = cuNameTranxMap.begin();
      for (; cuIter != cuNameTranxMap.end(); ++cuIter) {
        std::string cuName = cuIter->first;

        uint64_t totalReadTranx  = 0;
        uint64_t totalWriteTranx = 0;
        if (HostSlotIndex == 0)
          s = numHostSlots;
        else 
          s = 0;
        for (; s < numSlots; ++s) {
          if (s == HostSlotIndex)
            continue;

          std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
          std::string cuSlotName = cuPortName.substr(0, cuPortName.find_first_of("/"));

          if (cuSlotName == cuName) {
            totalReadTranx  += counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                               + (rolloverCounts.ReadTranx[s] * 4294967296UL);
            totalWriteTranx += counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                               + (rolloverCounts.WriteTranx[s] * 4294967296UL);
          }
        }

        cuNameTranxMap[cuName] = (totalReadTranx + totalWriteTranx);
      }

      // Sort the CUs by their tranx count
      std::vector<std::pair<std::string, uint64_t>> cuPairs(cuNameTranxMap.begin(),
          cuNameTranxMap.end());
      std::sort(cuPairs.begin(), cuPairs.end(),
          [](const std::pair<std::string, uint64_t>& A, const std::pair<std::string, uint64_t>& B) {
               return (A.second > B.second);
             });

      // Now report them in order of total tranx counts
      for (const auto &pair : cuPairs) {
        std::string cuName = pair.first;

        uint64_t totalReadBytes  = 0;
        uint64_t totalWriteBytes = 0;
        uint64_t totalReadTranx  = 0;
        uint64_t totalWriteTranx = 0;
        if (HostSlotIndex == 0)
          s = numHostSlots;
        else 
          s = 0;
        for (; s < numSlots; ++s) {
          if (s == HostSlotIndex)
            continue;

          std::string cuPortName = DeviceBinaryDataSlotsMap.at(key)[s];
          std::string cuSlotName = cuPortName.substr(0, cuPortName.find_first_of("/"));

          if (cuSlotName == cuName) {
            totalReadBytes  += counterResults.ReadBytes[s] + rolloverResults.ReadBytes[s]
                               + (rolloverCounts.ReadBytes[s] * 4294967296UL);
            totalWriteBytes += counterResults.WriteBytes[s] + rolloverResults.WriteBytes[s]
                               + (rolloverCounts.WriteBytes[s] * 4294967296UL);
            totalReadTranx  += counterResults.ReadTranx[s] + rolloverResults.ReadTranx[s]
                               + (rolloverCounts.ReadTranx[s] * 4294967296UL);
            totalWriteTranx += counterResults.WriteTranx[s] + rolloverResults.WriteTranx[s]
                               + (rolloverCounts.WriteTranx[s] * 4294967296UL);
          }
        }

        double totalCUTimeMsec = PerfCounters.getComputeUnitTotalTime(deviceName, cuName);

        PerfCounters.writeTopKernelTransferSummary(writer, deviceName, cuName, totalWriteBytes,
            totalReadBytes, totalWriteTranx, totalReadTranx, totalCUTimeMsec, totalCUTimeMsec,
            maxBytesPerTransfer, maxTransferRateMBps);
      }
    }
  }

  void RTProfile::writeDeviceTransferSummary(WriterI* writer) const
  {
    PerfCounters.writeDeviceTransferSummary(writer, true);
    PerfCounters.writeDeviceTransferSummary(writer, false);
  }

  void RTProfile::writeTopDataTransferSummary(WriterI* writer, bool isRead) const
  {
    PerfCounters.writeTopDataTransferSummary(writer, isRead);
  }

  void RTProfile::writeTopDeviceTransferSummary(WriterI* writer, bool isRead) const
  {
    PerfCounters.writeTopDeviceTransferSummary(writer, isRead);
  }

  void RTProfile::getProfileRuleCheckSummary()
  {
    RuleChecks->getProfileRuleCheckSummary(this);
  }

  void RTProfile::writeProfileRuleCheckSummary(WriterI* writer)
  {
    RuleChecks->writeProfileRuleCheckSummary(writer, this);
  }

  void RTProfile::writeProfileSummary() {
    if(!this->isApplicationProfileOn())
      return;

    for (auto w : Writers) {
      w->writeSummary(this);
    }
  }

  // Add to the active devices.
  // Called thru device::load_program in xocl/core/device.cpp
  void RTProfile::addToActiveDevices(const std::string& deviceName)
  {
    XDP_LOG("addToActiveDevices: device = %s\n", deviceName.c_str());

    // Catch when a Zynq device is used
    if (deviceName.find("zcu102") != std::string::npos) {
      XDP_LOG("addToActiveDevices: found Zynq device!\n");
      IsZynq = true;
    }

    ActiveDevices.insert(deviceName);

    // Store arguments and banks for each CU and its ports
    setArgumentsBank(deviceName);
  }

  // Detect if given device is active or not
  bool RTProfile::isDeviceActive(const std::string& deviceName) const
  {
    if (deviceName.empty())
      return false;

    if (ActiveDevices.find(deviceName) != ActiveDevices.end())
      return true;

    return false;
  }

  // Return string that includes all active devices
  std::string RTProfile::getDeviceNames() const
  {
    return getDeviceNames(", ");
  }

  std::string RTProfile::getDeviceNames(const std::string& sep) const
  {
    std::string deviceNames;
    auto platform = XCL::RTSingleton::Instance()->getcl_platform_id();
    bool firstDevice = true;
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      if (!isDeviceActive(deviceName))
        continue;

      if (firstDevice) {
        firstDevice = false;
        deviceNames += deviceName;
      }
      else {
        deviceNames += sep + deviceName;
      }
    }
    return deviceNames;
  }

  // Set the mapping of APM slot number to CU name
  void RTProfile::setSlotComputeUnitName(int slotnum, const std::string& cuName)
  {
    SlotComputeUnitNameMap[slotnum] = cuName;
  }

  // Given the APM slot number, get the CU and kernel names
  void RTProfile::getSlotNames(int slotnum, std::string& cuName, std::string& kernelName) const
  {
    // Get CU name
    auto iter1 = SlotComputeUnitNameMap.find(slotnum);
    if (iter1 != SlotComputeUnitNameMap.end())
      cuName = iter1->second;
    else
      cuName = "slot" + std::to_string(slotnum);

    // Get kernel name
    getKernelFromComputeUnit(cuName, kernelName);
  }

  bool RTProfile::getLoggingTrace(int index)
  {
    if (index < XCL_PERF_MON_TOTAL_PROFILE) {
      return mLoggingTrace[index];
    }
    else {
      return false;
    }
  }

  void RTProfile::setLoggingTrace(int index, bool value)
  {
    if (index < XCL_PERF_MON_TOTAL_PROFILE)
      mLoggingTrace[index] = value;
  }

  uint64_t RTProfile::getLoggingTraceUsec() 
  {
    return mLoggingTraceUsec;
  }

  void RTProfile::setLoggingTraceUsec(uint64_t value)
  {
    mLoggingTraceUsec = value;
  }

  void RTProfile::setArgumentsBank(const std::string& deviceName)
  {
    auto rts = XCL::RTSingleton::Instance();
    auto platform = rts->getcl_platform_id();
    const std::string numerical("0123456789");

    for (auto device_id : platform->get_device_range()) {
      std::string currDevice = device_id->get_unique_name();
      XOCL_DEBUGF("setArgumentsBank: current device = %s, # CUs = %d\n",
          currDevice.c_str(), device_id->get_num_cus());
      if (currDevice.find(deviceName) == std::string::npos)
        continue;

      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto currCU = cu->get_name();
        auto currSymbol = cu->get_symbol();
        //XOCL_DEBUGF("setArgumentsBank: current CU = %s\n", currCU.c_str());

        // Compile set of ports on this CU
        std::set<std::string> portSet;
        for (auto arg : currSymbol->arguments) {
          if ((arg.address_qualifier == 0)
              || (arg.atype != xocl::xclbin::symbol::arg::argtype::indexed))
            continue;

          auto portName = arg.port;
          std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);
          portSet.insert(portName);
        }

        // Now find all arguments for each port
        for (auto portName : portSet) {
          CUPortArgsBankType row;
          std::get<0>(row) = currCU;
          std::get<1>(row) = portName;

          bool firstArg = true;
          uint32_t ddrBank = 0;

          for (auto arg : currSymbol->arguments) {
            auto currPort = arg.port;
            std::transform(currPort.begin(), currPort.end(), currPort.begin(), ::tolower);
            //XOCL_DEBUGF("setArgumentsBank: name = %s, aq = %d, atype = %d\n",
            //    arg.name.c_str(), arg.address_qualifier, arg.atype);

            if ((currPort == portName) && (arg.address_qualifier == 1)
                && (arg.atype == xocl::xclbin::symbol::arg::argtype::indexed)) {
              std::get<2>(row) += (firstArg) ? arg.name : ("|" + arg.name);

              auto portWidth = arg.port_width;
              unsigned long index = (unsigned long)std::stoi(arg.id);
              //XOCL_DEBUGF("setArgumentsBank: getting bank for index %d\n", index);

              try {
                // TODO: deal with arguments connected to multiple memory banks
                // TODO: store DDR bank as a string not an integer!
                auto memidx_mask = cu->get_memidx(index);
                auto memidx = 0;
                for (auto memidx=0; memidx<memidx_mask.size(); ++memidx) {
                  if (memidx_mask.test(memidx)) {
                    // Get bank tag string from index
                    std::string ddrBankStr("bank0");
                    if (device_id->is_active()) {
                      ddrBankStr = device_id->get_xclbin().memidx_to_banktag(memidx);

                      auto found = ddrBankStr.find("]");
                      if (found != std::string::npos)
                        ddrBankStr.erase(found, 1);
                    }

                    // Convert to integer
                    auto found = ddrBankStr.find_last_not_of(numerical);
                    if (found != std::string::npos)
                      ddrBank = std::atoi(ddrBankStr.substr(found+1).c_str());

                    XOCL_DEBUGF("setArgumentsBank: idx = %d, str = %s, bank = %d\n", memidx, ddrBankStr.c_str(), ddrBank);
                    break;
                  }
                }
              }
              catch (const std::runtime_error& ex) {
                XOCL_DEBUGF("setArgumentsBank: caught error, using default of bank 0\n");
                ddrBank = 0;
              }

              std::get<3>(row) = ddrBank;
              std::get<4>(row) = portWidth;
              firstArg = false;
            }
          }

          // Increment total CU ports connected to this DDR bank
          ddrBank = (ddrBank >= MAX_DDR_BANKS) ? MAX_DDR_BANKS-1 : ddrBank;
          CUPortsToDDRBanks[ddrBank]++;

          XOCL_DEBUGF("setArgumentsBank: %s/%s, args = %s, bank = %d, width = %d\n",
              std::get<0>(row).c_str(), std::get<1>(row).c_str(), std::get<2>(row).c_str(),
              std::get<3>(row), std::get<4>(row));
          CUPortVector.push_back(row);
        }

        portSet.clear();
      } // for cu
    } // for device_id
  }

  void RTProfile::getArgumentsBank(const std::string& deviceName, const std::string& cuName,
	                               const std::string& portName, std::string& argNames,
								   uint32_t& banknum) const
  {
    argNames = "All";
    banknum = 0;

    //XOCL_DEBUGF("getArgumentsBank: %s/%s\n", cuName.c_str(), portName.c_str());

    // Find CU and port, then capture arguments and bank
    for (auto row : CUPortVector) {
      std::string currCU   = std::get<0>(row);
      std::string currPort = std::get<1>(row);

      if ((currCU == cuName) && (currPort == portName)) {
        argNames  = std::get<2>(row);
        banknum   = std::get<3>(row);
        break;
      }
    }
  }

  //
  // Used by PRCs
  //
  double RTProfile::getDeviceStartTime(const std::string& deviceName) const {
    return PerfCounters.getDeviceStartTime(deviceName);
  }

  double RTProfile::getTotalKernelExecutionTime(const std::string& deviceName) const {
    return PerfCounters.getTotalKernelExecutionTime(deviceName);
  }

  uint32_t RTProfile::getComputeUnitCalls(const std::string& deviceName, const std::string& cuName) const {
    return PerfCounters.getComputeUnitCalls(deviceName, cuName);
  }

  void RTProfile::getKernelFromComputeUnit(const std::string& cuName, std::string& kernelName) const {
    auto iter = ComputeUnitKernelNameMap.find(cuName);
    if (iter != ComputeUnitKernelNameMap.end())
      kernelName = iter->second;
    else
      kernelName = CurrentKernelName;
  }

  void RTProfile::getTraceStringFromComputeUnit(const std::string& deviceName, const std::string& cuName, std::string& traceString) const {
    auto iter = ComputeUnitKernelTraceMap.find(cuName);
    if (iter != ComputeUnitKernelTraceMap.end()) {
      traceString = iter->second;
    }
    else {
      // CR 1003380 - Runtime does not send all CU Names so we create a key
        std::string kernelName;
        XCL::RTSingleton::Instance()->getProfileKernelName(deviceName, cuName, kernelName);
        for (const auto &pair : ComputeUnitKernelTraceMap) {
        auto fullName = pair.second;
        auto first_index = fullName.find_first_of("|");
        auto second_index = fullName.find('|', first_index+1);
        auto third_index = fullName.find('|', second_index+1);
        auto fourth_index = fullName.find("|", third_index+1);
        auto fifth_index = fullName.find("|", fourth_index+1);
        auto sixth_index = fullName.find_last_of("|");
        std::string currKernelName = fullName.substr(third_index + 1, fourth_index - third_index - 1);
        if (currKernelName == kernelName) {
          traceString = fullName.substr(0,fifth_index + 1) + cuName + fullName.substr(sixth_index);
          return;
        }
      }
      traceString = std::string();
    }
  }
}


