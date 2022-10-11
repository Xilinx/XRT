/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include <map>
#include <mutex>

#define XDP_SOURCE

#include "core/common/time.h"
#include "xdp/profile/database/events/native_events.h"
#include "xdp/profile/plugin/native/native_cb.h"
#include "xdp/profile/plugin/native/native_plugin.h"

namespace xdp {

  static NativeProfilingPlugin nativePluginInstance ;

  // For logging statistics: Function ID to start timestamp
  static std::mutex timestampLock ;
  static std::map<uint64_t, uint64_t> nativeTimestamps ;

} // end namespace xdp

extern "C"
void native_function_start(const char* functionName, unsigned long long int functionID)
{
  if (!xdp::VPDatabase::alive() || !xdp::NativeProfilingPlugin::alive())
    return;

  // Don't include the profiling overhead in the time that we show.
  //  That means there will be "empty gaps" in the timeline trace when
  //  the profiling overhead exists.
  xdp::VPDatabase* db = xdp::nativePluginInstance.getDatabase() ;

  xdp::VTFEvent* event =
    new xdp::NativeAPICall(0,
                           0,
                           (db->getDynamicInfo()).addString(functionName)) ;
  (db->getDynamicInfo()).addUnsortedEvent(event);
  (db->getDynamicInfo()).markStart(static_cast<uint64_t>(functionID), event->getEventId()) ;

  db->getStats().logFunctionCallStart(functionName, static_cast<double>(xrt_core::time_ns()));
  event->setTimestamp(static_cast<double>(xrt_core::time_ns())) ;
}

extern "C"
void native_function_end(const char* functionName, unsigned long long int functionID, unsigned long long int timestamp)
{
  if (!xdp::VPDatabase::alive() || !xdp::NativeProfilingPlugin::alive())
    return;

  xdp::VPDatabase* db = xdp::nativePluginInstance.getDatabase() ;
  db->getStats().logFunctionCallEnd(functionName, static_cast<double>(timestamp)) ;

  uint64_t start =
    (db->getDynamicInfo()).matchingStart(static_cast<uint64_t>(functionID)) ;

  xdp::VTFEvent* event =
    new xdp::NativeAPICall(start,
                           static_cast<double>(timestamp),
                           (db->getDynamicInfo()).addString(functionName)) ;
  (db->getDynamicInfo()).addUnsortedEvent(event) ;
}

extern "C"
void native_sync_start(const char* functionName, unsigned long long int functionID, bool isWrite)
{
  if (!xdp::VPDatabase::alive() || !xdp::NativeProfilingPlugin::alive())
    return;

  // Don't include the profiling overhead in the time that we show.
  //  That means there will be "empty gaps" in the timeline trace when
  //  the profiling overhead exists.
  xdp::VPDatabase* db = xdp::nativePluginInstance.getDatabase() ;

  xdp::VTFEvent* event = nullptr ;
  if (isWrite) {
    event =
      new xdp::NativeSyncWrite(0,
                               0,
                               db->getDynamicInfo().addString(functionName),
                               db->getDynamicInfo().addString("WRITE")) ;
  }
  else {
    event =
      new xdp::NativeSyncRead(0,
                              0,
                              db->getDynamicInfo().addString(functionName),
                              db->getDynamicInfo().addString("READ")) ;
  }

  (db->getDynamicInfo()).addUnsortedEvent(event);
  (db->getDynamicInfo()).markStart(static_cast<uint64_t>(functionID), event->getEventId()) ;

  {
    std::lock_guard<std::mutex> lock(xdp::timestampLock) ;
    xdp::nativeTimestamps[static_cast<uint64_t>(functionID)] = xrt_core::time_ns() ;
  }

  db->getStats().logFunctionCallStart(functionName, static_cast<double>(xrt_core::time_ns()));
  event->setTimestamp(static_cast<double>(xrt_core::time_ns())) ;
}

extern "C"
void native_sync_end(const char* functionName, unsigned long long int functionID, unsigned long long int timestamp, bool isWrite, unsigned long long int size)
{
  if (!xdp::VPDatabase::alive() || !xdp::NativeProfilingPlugin::alive())
    return;

  xdp::VPDatabase* db = xdp::nativePluginInstance.getDatabase() ;
  db->getStats().logFunctionCallEnd(functionName, static_cast<double>(timestamp)) ;

  uint64_t startTimestamp = 0 ;
  uint64_t transferTime = 0 ;
  {
    std::lock_guard<std::mutex> lock(xdp::timestampLock) ;
    startTimestamp = xdp::nativeTimestamps[static_cast<uint64_t>(functionID)] ;
    transferTime = timestamp - startTimestamp ;
    xdp::nativeTimestamps.erase(functionID) ;
  }

  uint64_t start =
    (db->getDynamicInfo()).matchingStart(static_cast<uint64_t>(functionID)) ;

  xdp::VTFEvent* event = nullptr ;
  if (isWrite) {
    event =
      new xdp::NativeSyncWrite(start,
                               static_cast<double>(timestamp),
                               db->getDynamicInfo().addString(functionName),
                               db->getDynamicInfo().addString("WRITE")) ;
  }
  else {
    event =
      new xdp::NativeSyncRead(start,
                              static_cast<double>(timestamp),
                              db->getDynamicInfo().addString(functionName),
                              db->getDynamicInfo().addString("READ")) ;
  }
  (db->getDynamicInfo()).addUnsortedEvent(event) ;

  if (isWrite) {
    db->getStats().logHostWrite(0, 0, size, startTimestamp, transferTime, 0, 0);
  }
  else {
    db->getStats().logHostRead(0, 0, size, startTimestamp, transferTime, 0, 0);
  }
}
