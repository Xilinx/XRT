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

#include "xdp/profile/plugin/lop/lop_cb.h"
#include "xdp/profile/plugin/lop/lop_plugin.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/opencl_api_calls.h"
#include "xdp/profile/database/events/opencl_host_events.h"
#include "xrt/util/time.h"

namespace xdp {

  static LowOverheadProfilingPlugin lopPluginInstance ;

  static void lop_cb_log_function_start(const char* functionName,
                                        long long queueAddress,
                                        unsigned long long int functionID)
  {
    if (!VPDatabase::alive() || !LowOverheadProfilingPlugin::alive())
      return ;

    // Since these are OpenCL level events, we must use the OpenCL
    //  level time functions to get the proper value of time zero.
    auto timestamp = static_cast<double>(xrt_xocl::time_ns());
    VPDatabase* db = lopPluginInstance.getDatabase() ;

    if (queueAddress != 0) 
      (db->getStaticInfo()).addCommandQueueAddress(queueAddress) ;

    VTFEvent* event = new OpenCLAPICall(0,
                                        timestamp,
                                        functionID,
                                        (db->getDynamicInfo()).addString(functionName),
                                        queueAddress,
                                        true); // is Low Overhead
    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).markStart(functionID, event->getEventId()) ;
  }

  static void lop_cb_log_function_end(const char* functionName,
                                      long long queueAddress,
                                      unsigned long long int functionID)
  {
    if (!VPDatabase::alive() || !LowOverheadProfilingPlugin::alive())
      return ;

    auto timestamp = static_cast<double>(xrt_xocl::time_ns());
    VPDatabase* db = lopPluginInstance.getDatabase() ;

    uint64_t start = (db->getDynamicInfo()).matchingStart(functionID) ;

    VTFEvent* event = new OpenCLAPICall(start,
                                        timestamp,
                                        functionID,
                                        (db->getDynamicInfo()).addString(functionName),
                                        queueAddress,
                                        true) ; // is Low Overhead
    (db->getDynamicInfo()).addEvent(event) ;
  }

  static void lop_read(unsigned int XRTEventId, bool isStart)
  {
    if (!VPDatabase::alive() || !LowOverheadProfilingPlugin::alive())
      return ;

    auto timestamp = static_cast<double>(xrt_xocl::time_ns());
    VPDatabase* db = lopPluginInstance.getDatabase() ;
    
    uint64_t start = 0 ;
    uint64_t lopEventId = LOP_EVENT_MASK | (uint64_t)(XRTEventId) ;
    if (!isStart) start = (db->getDynamicInfo()).matchingStart(lopEventId) ;

    VTFEvent* event = new LOPBufferTransfer(start,
                                            timestamp,
                                            LOP_READ_BUFFER) ;

    (db->getDynamicInfo()).addEvent(event) ;
    if (isStart)
      (db->getDynamicInfo()).markStart(lopEventId, event->getEventId()) ;
  }

  static void lop_write(unsigned int XRTEventId, bool isStart)
  {
    if (!VPDatabase::alive() || !LowOverheadProfilingPlugin::alive())
      return ;

    auto timestamp = static_cast<double>(xrt_xocl::time_ns());
    VPDatabase* db = lopPluginInstance.getDatabase() ;

    uint64_t start = 0 ;
    uint64_t lopEventId = LOP_EVENT_MASK | (uint64_t)(XRTEventId) ;
    if (!isStart) start = (db->getDynamicInfo()).matchingStart(lopEventId) ;

    VTFEvent* event = new LOPBufferTransfer(start,
                                            timestamp,
                                            LOP_WRITE_BUFFER) ;
    (db->getDynamicInfo()).addEvent(event) ;
    if (isStart)
      (db->getDynamicInfo()).markStart(lopEventId, event->getEventId()) ;
  }

  static void lop_kernel_enqueue(unsigned int XRTEventId, bool isStart)
  {
    if (!VPDatabase::alive() || !LowOverheadProfilingPlugin::alive())
      return ;

    auto timestamp = static_cast<double>(xrt_xocl::time_ns());
    VPDatabase* db = lopPluginInstance.getDatabase() ;

    uint64_t start = 0 ;
    uint64_t lopEventId = LOP_EVENT_MASK | (uint64_t)(XRTEventId) ;
    if (!isStart) start = (db->getDynamicInfo()).matchingStart(lopEventId) ;

    VTFEvent* event = new LOPKernelEnqueue(start, timestamp) ;

    (db->getDynamicInfo()).addEvent(event) ;
    if (isStart)
      (db->getDynamicInfo()).markStart(lopEventId, event->getEventId()) ;
  }

} // end namespace xdp

// Due to an issue with linking on Ubuntu 18.04, the model we have
//  to have for low overhead profiling is to have XRT use dlsym to
//  look up our functions and call them directly.  There will be no
//  registering of callbacks here.

extern "C"
void lop_function_start(const char* functionName, long long queueAddress,
                        unsigned long long int functionID)
{
  xdp::lop_cb_log_function_start(functionName, queueAddress, functionID) ;
}

extern "C"
void lop_function_end(const char* functionName, long long queueAddress,
                      unsigned long long int functionID)
{
  xdp::lop_cb_log_function_end(functionName, queueAddress, functionID) ;
}

extern "C"
void lop_read(unsigned int XRTEventId, bool isStart)
{
  xdp::lop_read(XRTEventId, isStart) ;
}

extern "C"
void lop_write(unsigned int XRTEventId, bool isStart)
{
  xdp::lop_write(XRTEventId, isStart) ;
}

extern "C"
void lop_kernel_enqueue(unsigned int XRTEventId, bool isStart) 
{
  xdp::lop_kernel_enqueue(XRTEventId, isStart) ;
}
