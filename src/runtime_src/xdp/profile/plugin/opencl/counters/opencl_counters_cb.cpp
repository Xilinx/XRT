/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include <map>
#include <queue>
#include <mutex>

#include "core/common/time.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/opencl/counters/opencl_counters_cb.h"
#include "xdp/profile/plugin/opencl/counters/opencl_counters_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  static OpenCLCountersProfilingPlugin openclCountersPluginInstance ;

  static void log_function_call_start(const char* functionName, uint64_t queueAddress, bool isOOO)
  {
    if (!VPDatabase::alive() || !OpenCLCountersProfilingPlugin::alive())
      return;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    auto timestamp = static_cast<double>(xrt_core::time_ns());

    (db->getStats()).logFunctionCallStart(functionName, timestamp) ;
    if (queueAddress != 0)
      (db->getStats()).setCommandQueueOOO(queueAddress, isOOO) ;
  }

  static void log_function_call_end(const char* functionName)
  {
    if (!VPDatabase::alive() || !OpenCLCountersProfilingPlugin::alive())
      return;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    auto timestamp = static_cast<double>(xrt_core::time_ns());

    (db->getStats()).logFunctionCallEnd(functionName, timestamp) ;
  }

  static void log_kernel_execution(const char* kernelName,
                                   bool isStart,
                                   uint64_t kernelInstanceAddress,
                                   uint64_t contextId,
                                   uint64_t commandQueueId,
                                   const char* deviceName,
                                   const char* globalWorkSize,
                                   const char* localWorkSize,
                                   const char** buffers,
                                   uint64_t numBuffers)
  {
    if (!VPDatabase::alive() || !OpenCLCountersProfilingPlugin::alive())
      return;

    static std::map<std::string, std::queue<uint64_t> > storedTimestamps ;
    static std::mutex timestampLock ;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    uint64_t timestamp = xrt_core::time_ns() ;

    if (getFlowMode() == HW_EMU)
      timestamp =
        openclCountersPluginInstance.convertToEstimatedTimestamp(timestamp) ;

    // Since we don't have device information in software emulation,
    //  we have to piggyback this information here.
    if (getFlowMode() == SW_EMU)
      (db->getStaticInfo()).setSoftwareEmulationDeviceName(deviceName) ;

    std::lock_guard<std::mutex> lock(timestampLock) ;
    if (isStart) {
      (storedTimestamps[kernelName]).push(timestamp) ;
      
      // Also, for guidance, keep track of the total number of concurrent
      (db->getStats()).logMaxExecutions(kernelName,
                                        storedTimestamps[kernelName].size()) ;
    }
    else {
      if (storedTimestamps[kernelName].size() <= 0) {
        // There are times we get ends with no corresponding starts.
        //  We can just ignore them.
        return ; 
      }
      uint64_t startTime = storedTimestamps[kernelName].front() ;
      (storedTimestamps[kernelName]).pop() ;
      auto executionTime = timestamp-startTime;

      (db->getStats()).logDeviceActiveTime(deviceName, startTime, timestamp) ;
      (db->getStats()).logKernelExecution(kernelName,
                                          executionTime,
                                          kernelInstanceAddress,
                                          contextId,
                                          commandQueueId,
                                          deviceName,
                                          startTime,
                                          globalWorkSize,
                                          localWorkSize,
                                          buffers,
                                          numBuffers) ;
    }
  }

  static void log_compute_unit_execution(const char* cuName,
                                         const char* kernelName,
                                         const char* localWorkGroup,
                                         const char* globalWorkGroup,
                                         bool isStart)
  {
    if (!VPDatabase::alive() || !OpenCLCountersProfilingPlugin::alive())
      return;

    // Log the execution of a compute unit
    // NOTE: This is only valid for SW emulation. For HW and HW emulation, only the
    // scheduler knows which CU gets the job. For those flows, we need to get the
    // CU execution times from trace as read from accelerator monitors on the device. 
    if (getFlowMode() != SW_EMU)
      return;

    static std::map<std::tuple<std::string, std::string, std::string>, 
                    uint64_t> storedTimestamps ;
    static std::mutex timestampLock ;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    uint64_t timestamp = xrt_core::time_ns() ;

    std::tuple<std::string, std::string, std::string> combinedName =
      std::make_tuple(cuName, localWorkGroup, globalWorkGroup) ;

    std::lock_guard<std::mutex> lock(timestampLock) ;
    if (isStart)
      storedTimestamps[combinedName] = timestamp ;
    else {
      auto executionTime = timestamp - storedTimestamps[combinedName] ;
      storedTimestamps.erase(combinedName) ;

      (db->getStats()).logComputeUnitExecution(cuName,
                                               kernelName,
                                               localWorkGroup,
                                               globalWorkGroup,
                                               executionTime);
    }
  }

  static void counter_action_read(uint64_t contextId,
                                  uint64_t numDevices,
                                  const char* deviceName,
                                  uint64_t eventId,
                                  uint64_t size,
                                  bool isStart,
                                  bool isP2P,
                                  uint64_t address,
                                  uint64_t commandQueueId)
  {
    if (!VPDatabase::alive() || !OpenCLCountersProfilingPlugin::alive())
      return;

    static std::map<std::pair<uint64_t, std::string>, uint64_t> 
      storedTimestamps ;
    static std::mutex timestampLock ;

    std::pair<uint64_t, std::string> identifier =
      std::make_pair(eventId, std::string(deviceName)) ;

    // clEnqueueNDRangeKernel will issue end events with no start
    //  if the data transfer didn't have to happen.  We can safely
    //  discard those events, so just return
    if (!isStart && storedTimestamps.find(identifier) == storedTimestamps.end())
      return ;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    uint64_t timestamp = xrt_core::time_ns() ;

    // For total active buffer transfer time
    if (db->getStats().getTotalBufferStartTime() == 0)
      (db->getStats()).setTotalBufferStartTime(timestamp) ;
    (db->getStats()).setTotalBufferEndTime(timestamp) ;

    std::lock_guard<std::mutex> lock(timestampLock) ;
    if (isStart)
      storedTimestamps[identifier] = timestamp ;
    else {
      uint64_t startTime = storedTimestamps[identifier] ;
      storedTimestamps.erase(identifier) ;
      uint64_t transferTime = timestamp - startTime ;

      uint64_t deviceId = 0 ; // TODO - lookup the device ID from device name

      (db->getStats()).logHostRead(contextId, deviceId, size, startTime, transferTime, address, commandQueueId) ;
      if (isP2P) (db->getStats()).addHostP2PTransfer() ;
    }
    (db->getStaticInfo()).setNumDevices(contextId, numDevices) ;
  }

  static void counter_action_write(uint64_t contextId,
                                   const char* deviceName,
                                   uint64_t eventId,
                                   uint64_t size,
                                   bool isStart,
                                   bool isP2P,
                                   uint64_t address,
                                   uint64_t commandQueueId)
  {
    if (!VPDatabase::alive() || !OpenCLCountersProfilingPlugin::alive())
      return;

    static std::map<std::pair<uint64_t, std::string>, uint64_t> 
      storedTimestamps ;
    static std::mutex timestampLock ;

    std::pair<uint64_t, std::string> identifier =
      std::make_pair(eventId, std::string(deviceName)) ;

    // clEnqueueNDRangeKernel will issue end events with no start
    //  if the data transfer didn't have to happen.  We can safely
    //  discard those events, so just return
    if (!isStart && storedTimestamps.find(identifier) == storedTimestamps.end())
      return ;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    uint64_t timestamp = xrt_core::time_ns() ;

    // For total active buffer transfer time
    if (db->getStats().getTotalBufferStartTime() == 0)
      (db->getStats()).setTotalBufferStartTime(timestamp) ;
    (db->getStats()).setTotalBufferEndTime(timestamp) ;

    std::lock_guard<std::mutex> lock(timestampLock) ;
    if (isStart)
      storedTimestamps[identifier] = timestamp ;
    else {
      uint64_t startTime = storedTimestamps[identifier] ;
      storedTimestamps.erase(identifier) ;

      uint64_t transferTime = timestamp - startTime ;

      uint64_t deviceId = 0 ; // TODO - lookup the device ID from device name

      (db->getStats()).logHostWrite(contextId, deviceId, size, startTime, transferTime, address, commandQueueId) ;
      if (isP2P) (db->getStats()).addHostP2PTransfer() ;
    }
  }

  static void counter_mark_objects_released()
  {
    if (!VPDatabase::alive() || !OpenCLCountersProfilingPlugin::alive())
      return;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    (db->getStats()).addOpenCLObjectReleased() ;
  }

} // end namespace xdp

extern "C"
void log_function_call_start(const char* functionName,
                             unsigned long long int queueAddress,
                             bool isOOO)
{
  xdp::log_function_call_start(functionName,
                               static_cast<uint64_t>(queueAddress),
                               isOOO) ;
}

extern "C"
void log_function_call_end(const char* functionName)
{
  xdp::log_function_call_end(functionName) ;
}

extern "C"
void log_kernel_execution(const char* kernelName,
                          bool isStart,
                          unsigned long long int kernelInstanceId,
                          unsigned long long int contextId,
                          unsigned long long int commandQueueId,
                          const char* deviceName,
                          const char* globalWorkSize,
                          const char* localWorkSize,
                          const char** buffers,
                          unsigned long long int numBuffers)
{
  xdp::log_kernel_execution(kernelName,
                            isStart,
                            static_cast<uint64_t>(kernelInstanceId),
                            static_cast<uint64_t>(contextId),
                            static_cast<uint64_t>(commandQueueId),
                            deviceName,
                            globalWorkSize,
                            localWorkSize,
                            buffers,
                            static_cast<uint64_t>(numBuffers)) ;
}

extern "C"
void log_compute_unit_execution(const char* cuName,
                                const char* kernelName,
                                const char* localWorkGroupConfiguration,
                                const char* globalWorkGroupConfiguration,
                                bool isStart)
{
  xdp::log_compute_unit_execution(cuName,
                                  kernelName,
                                  localWorkGroupConfiguration,
                                  globalWorkGroupConfiguration,
                                  isStart) ;
}

extern "C"
void counter_action_read(unsigned long long int contextId,
                         unsigned long long int numDevices,
                         const char* deviceName,
                         unsigned long long int eventId,
                         unsigned long long int size,
                         bool isStart,
                         bool isP2P,
                         unsigned long long int address,
                         unsigned long long int commandQueueId)
{
  xdp::counter_action_read(static_cast<uint64_t>(contextId),
                           static_cast<uint64_t>(numDevices),
                           deviceName,
                           static_cast<uint64_t>(eventId),
                           static_cast<uint64_t>(size),
                           isStart,
                           isP2P,
                           static_cast<uint64_t>(address),
                           static_cast<uint64_t>(commandQueueId));
}

extern "C"
void counter_action_write(unsigned long long int contextId,
                          const char* deviceName,
                          unsigned long long int eventId,
                          unsigned long long int size,
                          bool isStart,
                          bool isP2P,
                          unsigned long long int address,
                          unsigned long long int commandQueueId)
{
  xdp::counter_action_write(static_cast<uint64_t>(contextId),
                            deviceName,
                            static_cast<uint64_t>(eventId),
                            static_cast<uint64_t>(size),
                            isStart,
                            isP2P,
                            static_cast<uint64_t>(address),
                            static_cast<uint64_t>(commandQueueId));
}

extern "C"
void counter_mark_objects_released()
{
  xdp::counter_mark_objects_released() ;
}
