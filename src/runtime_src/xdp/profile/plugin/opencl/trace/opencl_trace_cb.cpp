/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#include <iostream>

#include "core/common/time.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/opencl_api_calls.h"
#include "xdp/profile/database/events/opencl_host_events.h"
#include "xdp/profile/plugin/opencl/trace/opencl_trace_cb.h"
#include "xdp/profile/plugin/opencl/trace/opencl_trace_plugin.h"

namespace xdp {

  static OpenCLTracePlugin openclPluginInstance ;

  static void log_function_start(const char* functionName,
                                 uint64_t queueAddress,
                                 uint64_t functionID)
  {
    if (!VPDatabase::alive() || !OpenCLTracePlugin::alive())
      return;

    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    if (queueAddress != 0) 
      (db->getStaticInfo()).addCommandQueueAddress(queueAddress) ;

    VTFEvent* event = new OpenCLAPICall(0,
                                        timestamp,
                                        functionID,
                                        (db->getDynamicInfo()).addString(functionName),
                                        queueAddress
                                        ) ;
    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).markStart(functionID, event->getEventId()) ;
  }

  static void log_function_end(const char* functionName,
                               uint64_t queueAddress,
                               uint64_t functionID)
  {
    if (!VPDatabase::alive() || !OpenCLTracePlugin::alive())
      return;

    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    uint64_t start = (db->getDynamicInfo()).matchingStart(functionID) ;

    VTFEvent* event = new OpenCLAPICall(start,
                                        timestamp,
                                        functionID,
                                        (db->getDynamicInfo()).addString(functionName),
                                        queueAddress) ;
    (db->getDynamicInfo()).addEvent(event) ;
  }

  // The XRT event "id" cannot start until the XRT event "dependency" has ended
  static void add_dependency(uint64_t id, uint64_t dependency)
  {
    if (!VPDatabase::alive() || !OpenCLTracePlugin::alive())
      return;

    VPDatabase* db = openclPluginInstance.getDatabase() ;
    (db->getDynamicInfo()).addDependency(id, dependency) ;
  }

  static void action_read(uint64_t id,
                          bool isStart,
                          uint64_t deviceAddress,
                          const char* memoryResource,
                          size_t bufferSize,
                          bool isP2P)
  {
    if (!VPDatabase::alive() || !OpenCLTracePlugin::alive())
      return;

    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    uint64_t start = 0 ;
    
    if (!isStart) start = (db->getDynamicInfo()).matchingXRTUIDStart(id) ;

    VTFEvent* event = 
      new OpenCLBufferTransfer(start,
                               timestamp,
                               (isP2P ? READ_BUFFER_P2P : READ_BUFFER),
                               deviceAddress,
                               memoryResource ? (db->getDynamicInfo()).addString(memoryResource) : 0,
                               bufferSize) ;

    (db->getDynamicInfo()).addEvent(event) ;
    if (isStart) {
      (db->getDynamicInfo()).markXRTUIDStart(id, event->getEventId()) ;
    }
    else {
      (db->getDynamicInfo()).addOpenCLMapping(id, event->getEventId(), start);
    }
  }

  static void action_write(uint64_t id,
                           bool isStart,
                           uint64_t deviceAddress,
                           const char* memoryResource,
                           size_t bufferSize,
                           bool isP2P)
  {
    if (!VPDatabase::alive() || !OpenCLTracePlugin::alive())
      return;

    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    uint64_t start = 0 ;
    
    if (!isStart) start = (db->getDynamicInfo()).matchingXRTUIDStart(id) ;

    // On the OpenCL side, NDRange Migrate might generate buffer transfer
    //  complete events with a buffer size of 0 that don't have corresponding
    //  start events.  Don't keep track of these.
    if (!isStart && start == 0 && bufferSize == 0)
      return ;

    VTFEvent* event = 
      new OpenCLBufferTransfer(start,
                               timestamp,
                               (isP2P ? WRITE_BUFFER_P2P : WRITE_BUFFER),
                               deviceAddress,
                               memoryResource ? (db->getDynamicInfo()).addString(memoryResource) : 0,
                               bufferSize) ;

    (db->getDynamicInfo()).addEvent(event) ;
    if (isStart) {
      (db->getDynamicInfo()).markXRTUIDStart(id, event->getEventId()) ;
    }
    else {
      (db->getDynamicInfo()).addOpenCLMapping(id, event->getEventId(), start);
    }
  }

  static void action_copy(uint64_t id,
                          bool isStart,
                          uint64_t srcDeviceAddress,
                          const char* srcMemoryResource,
                          uint64_t dstDeviceAddress,
                          const char* dstMemoryResource,
                          size_t bufferSize,
                          bool isP2P)
  {
    if (!VPDatabase::alive() || !OpenCLTracePlugin::alive())
      return;

    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    uint64_t start = 0 ;
    
    if (!isStart) start = (db->getDynamicInfo()).matchingXRTUIDStart(id) ;

    VTFEvent* event = 
      new OpenCLCopyBuffer(start,
                           timestamp,
                           (isP2P ? COPY_BUFFER_P2P : COPY_BUFFER),
                           srcDeviceAddress,
                           srcMemoryResource ? (db->getDynamicInfo()).addString(srcMemoryResource) : 0,
                           dstDeviceAddress,
                           dstMemoryResource ? (db->getDynamicInfo()).addString(dstMemoryResource) : 0,
                           bufferSize) ;

    (db->getDynamicInfo()).addEvent(event) ;
    if (isStart) {
      (db->getDynamicInfo()).markXRTUIDStart(id, event->getEventId()) ;
    }
    else {
      (db->getDynamicInfo()).addOpenCLMapping(id, event->getEventId(), start);
    }
  }
  
  static void action_ndrange(uint64_t id,
                             bool isStart,
                             const char* deviceName,
                             const char* binaryName,
                             const char* kernelName,
                             size_t workgroupConfigurationX,
                             size_t workgroupConfigurationY,
                             size_t workgroupConfigurationZ,
                             size_t workgroupSize)
  {
    if (!VPDatabase::alive() || !OpenCLTracePlugin::alive())
      return;

    auto timestamp = static_cast<double>(xrt_core::time_ns());
    VPDatabase* db = openclPluginInstance.getDatabase() ;

    uint64_t start = 0 ;
    
    if (!isStart) start = (db->getDynamicInfo()).matchingXRTUIDStart(id) ;
    std::string workgroupConfiguration = 
      std::to_string(workgroupConfigurationX) + ":" +
      std::to_string(workgroupConfigurationY) + ":" +
      std::to_string(workgroupConfigurationZ) ;

    std::string enqueueIdentifier = "" ;

    if (deviceName != nullptr && binaryName != nullptr && kernelName != nullptr)
    {
      enqueueIdentifier = std::string(deviceName) + ":" +
                          std::string(binaryName) + ":" +
                          std::string(kernelName) ;
      (db->getStaticInfo()).addEnqueuedKernel(enqueueIdentifier) ;
    }

    VTFEvent* event = 
      new KernelEnqueue(start, 
                        timestamp,
                        deviceName ? (db->getDynamicInfo()).addString(deviceName) : 0,
                        binaryName ? (db->getDynamicInfo()).addString(binaryName) : 0,
                        kernelName ? (db->getDynamicInfo()).addString(kernelName) : 0,
                        (db->getDynamicInfo()).addString(workgroupConfiguration.c_str()),
                        workgroupSize,
                        enqueueIdentifier == "" ? nullptr : enqueueIdentifier.c_str()) ;

    (db->getDynamicInfo()).addEvent(event) ;

    if (isStart) {
      (db->getDynamicInfo()).markXRTUIDStart(id, event->getEventId()) ;
    }
    else {
      (db->getDynamicInfo()).addOpenCLMapping(id, event->getEventId(), start);
    }
  }

} // end namespace xdp

extern "C"
void function_start(const char* functionName, 
                    unsigned long long int queueAddress,
                    unsigned long long int functionID)
{
  xdp::log_function_start(functionName,
                          static_cast<uint64_t>(queueAddress),
                          static_cast<uint64_t>(functionID)) ;
}

extern "C"
void function_end(const char* functionName, 
                  unsigned long long int queueAddress,
                  unsigned long long int functionID)
{
  xdp::log_function_end(functionName,
                        static_cast<uint64_t>(queueAddress),
                        static_cast<uint64_t>(functionID)) ;
}

extern "C"
void add_dependency(unsigned long long int id,
                    unsigned long long int dependency)
{
  xdp::add_dependency(static_cast<uint64_t>(id),
                      static_cast<uint64_t>(dependency)) ;
}

extern "C"
void action_read(unsigned long long int id,
                 bool isStart,
                 unsigned long long int deviceAddress,
                 const char* memoryResource,
                 size_t bufferSize,
                 bool isP2P)
{
  xdp::action_read(static_cast<uint64_t>(id),
                   isStart,
                   static_cast<uint64_t>(deviceAddress),
                   memoryResource,
                   bufferSize, 
                   isP2P) ;
}

extern "C"
void action_write(unsigned long long int id,
                  bool isStart,
                  unsigned long long int deviceAddress,
                  const char* memoryResource,
                  size_t bufferSize,
                  bool isP2P)
{
  xdp::action_write(static_cast<uint64_t>(id),
                    isStart,
                    static_cast<uint64_t>(deviceAddress),
                    memoryResource,
                    bufferSize, 
                    isP2P) ;
}

extern "C"
void action_copy(unsigned long long int id,
                 bool isStart,
                 unsigned long long int srcDeviceAddress,
                 const char* srcMemoryResource,
                 unsigned long long int dstDeviceAddress,
                 const char* dstMemoryResource,
                 size_t bufferSize,
                 bool isP2P)
{
  xdp::action_copy(static_cast<uint64_t>(id),
                   isStart,
                   static_cast<uint64_t>(srcDeviceAddress),
                   srcMemoryResource, 
                   static_cast<uint64_t>(dstDeviceAddress),
                   dstMemoryResource,
                   bufferSize,
                   isP2P) ;
}

extern "C"
void action_ndrange(unsigned long long int id,
                    bool isStart,
                    const char* deviceName,
                    const char* binaryName,
                    const char* kernelName,
                    size_t workgroupConfigurationX,
                    size_t workgroupConfigurationY,
                    size_t workgroupConfigurationZ,
                    size_t workgroupSize)
{
  xdp::action_ndrange(static_cast<uint64_t>(id),
                      isStart,
                      deviceName,
                      binaryName,
                      kernelName,
                      workgroupConfigurationX,
                      workgroupConfigurationY,
                      workgroupConfigurationZ,
                      workgroupSize) ;
}
