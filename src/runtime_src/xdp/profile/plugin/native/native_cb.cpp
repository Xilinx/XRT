/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include "core/common/time.h"
#include "xdp/profile/database/dynamic_info/types.h"
#include "xdp/profile/database/events/native_events.h"
#include "xdp/profile/plugin/native/native_cb.h"
#include "xdp/profile/plugin/native/native_plugin.h"

namespace xdp {

  // The static instance of the plugin that is constructed when the
  // dynamic library is loaded.  It will be accessed by the callback
  // functions below.
  static NativeProfilingPlugin nativePluginInstance;

  // For logging statistics: Function ID to start timestamp
  static std::mutex timestampLock;
  static std::map<uint64_t, uint64_t> nativeTimestamps;

} // end namespace xdp

// The functionID is the unique identifier from the XRT side that we
// can use to match start events with stop events.
extern "C"
void native_function_start(const char* functionName,
                           unsigned long long int functionID)
{
  if (!xdp::VPDatabase::alive() || !xdp::NativeProfilingPlugin::alive())
    return;

  // Don't include the profiling overhead in the time that we show.
  // That means there will be "empty gaps" in the timeline trace when
  // the profiling overhead exists.  That means we create the event
  // and add it to the database first, and set the timestamp as close as
  // possible to the true start of the observed function.
  xdp::VPDatabase* db = xdp::nativePluginInstance.getDatabase();

  xdp::VTFEvent* event =
    new xdp::NativeAPICall(0,
                           0,
                           db->getDynamicInfo().addString(functionName));
  db->getDynamicInfo().addUnsortedEvent(event);
  db->getDynamicInfo().markStart(static_cast<uint64_t>(functionID),
                                 event->getEventId());

  db->getStats().logFunctionCallStart(functionName,
                                      static_cast<double>(xrt_core::time_ns()));
  event->setTimestamp(static_cast<double>(xrt_core::time_ns()));
}

// In order to not show profiling overhead in the timeline, we have
// already captured the timestamp when the observed function ended
// so any of the events we record do not take the local overhead into
// consideration.  The timestamp is as close to the true end of the
// observed function as possible.
extern "C"
void native_function_end(const char* functionName,
                         unsigned long long int functionID,
                         unsigned long long int timestamp)
{
  if (!xdp::VPDatabase::alive() || !xdp::NativeProfilingPlugin::alive())
    return;

  xdp::VPDatabase* db = xdp::nativePluginInstance.getDatabase();
  db->getStats().logFunctionCallEnd(functionName,
                                    static_cast<double>(timestamp));

  uint64_t start =
    db->getDynamicInfo().matchingStart(static_cast<uint64_t>(functionID));

  xdp::VTFEvent* event =
    new xdp::NativeAPICall(start,
                           static_cast<double>(timestamp),
                           db->getDynamicInfo().addString(functionName));
  db->getDynamicInfo().addUnsortedEvent(event);
}

// Callbacks for sync functions will create two separate events to be displayed
// on the visualization.  One that is put on the API row to show that
// xrt::sync was called, and one on the data transfer rows to show when
// reads and writes were occurring.
extern "C"
void native_sync_start(const char* functionName,
                       unsigned long long int functionID,
                       bool isWrite)
{
  if (!xdp::VPDatabase::alive() || !xdp::NativeProfilingPlugin::alive())
    return;

  // Don't include the profiling overhead in the time that we show.
  // That means there will be "empty gaps" in the timeline trace when
  // the profiling overhead exists.  We do this by capturing the
  // timestamp as close to the end of this function as possible
  xdp::VPDatabase* db = xdp::nativePluginInstance.getDatabase();

  // Create two different events.  One for capturing the API to be put
  // on the API row, and one for the read/write data transfer rows.
  xdp::VTFEvent* APIEvent      = nullptr;
  xdp::VTFEvent* transferEvent = nullptr;

  auto functionStr = db->getDynamicInfo().addString(functionName);
  APIEvent = new xdp::NativeAPICall(0, 0, functionStr);
  if (isWrite)
    transferEvent = new xdp::NativeSyncWrite(0, 0, functionStr);
  else
    transferEvent = new xdp::NativeSyncRead(0, 0, functionStr);

  db->getDynamicInfo().addUnsortedEvent(APIEvent);
  db->getDynamicInfo().addUnsortedEvent(transferEvent);

  // We need to store both events for lookup as we will only get one
  // "stop" event from the XRT side for this particular functionID.
  xdp::EventPair events = { APIEvent->getEventId(), transferEvent->getEventId() };
  db->getDynamicInfo().markEventPairStart(static_cast<uint64_t>(functionID), events);

  {
    // For statistics, also keep track of the start time associated with
    // this data transfer.
    std::lock_guard<std::mutex> lock(xdp::timestampLock);
    xdp::nativeTimestamps[static_cast<uint64_t>(functionID)] = xrt_core::time_ns();
  }

  db->getStats().logFunctionCallStart(functionName, static_cast<double>(xrt_core::time_ns()));
  APIEvent->setTimestamp(static_cast<double>(xrt_core::time_ns()));
  transferEvent->setTimestamp(static_cast<double>(xrt_core::time_ns()));
}

extern "C"
void native_sync_end(const char* functionName, unsigned long long int functionID, unsigned long long int timestamp, bool isWrite, unsigned long long int size)
{
  if (!xdp::VPDatabase::alive() || !xdp::NativeProfilingPlugin::alive())
    return;

  xdp::VPDatabase* db = xdp::nativePluginInstance.getDatabase();
  db->getStats().logFunctionCallEnd(functionName, static_cast<double>(timestamp));

  uint64_t startTimestamp = 0;
  uint64_t transferTime = 0;
  {
    std::lock_guard<std::mutex> lock(xdp::timestampLock);
    startTimestamp = xdp::nativeTimestamps[static_cast<uint64_t>(functionID)];
    transferTime = timestamp - startTimestamp;
    xdp::nativeTimestamps.erase(functionID);
  }

  // Retrieve the pair of events for this particular functionID.
  auto startEvents =
    db->getDynamicInfo().matchingEventPairStart(static_cast<uint64_t>(functionID));

  xdp::VTFEvent* APIEvent = nullptr;
  xdp::VTFEvent* transferEvent = nullptr;

  auto functionStr = db->getDynamicInfo().addString(functionName);

  APIEvent = new xdp::NativeAPICall(startEvents.APIEventId,
                                    static_cast<double>(timestamp),
                                    functionStr);

  if (isWrite) {
    transferEvent =
      new xdp::NativeSyncWrite(startEvents.transferEventId,
                               static_cast<double>(timestamp),
                               functionStr);
  }
  else {
    transferEvent =
      new xdp::NativeSyncRead(startEvents.transferEventId,
                              static_cast<double>(timestamp),
                              db->getDynamicInfo().addString(functionName));
  }
  db->getDynamicInfo().addUnsortedEvent(APIEvent);
  db->getDynamicInfo().addUnsortedEvent(transferEvent);

  if (isWrite)
    db->getStats().logHostWrite(0, 0, size, startTimestamp, transferTime, 0, 0);
  else
    db->getStats().logHostRead(0, 0, size, startTimestamp, transferTime, 0, 0);
}
