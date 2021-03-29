/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#include "xdp/profile/plugin/native/native_cb.h"
#include "xdp/profile/plugin/native/native_plugin.h"
#include "xdp/profile/database/events/native_events.h"

#include "core/common/time.h"

namespace xdp {

  static NativeProfilingPlugin nativePluginInstance ;

} // end namespace xdp

extern "C"
void native_function_start(const char* functionName, unsigned long long int functionID)
{
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

  event->setTimestamp(static_cast<double>(xrt_core::time_ns())) ;
}

extern "C"
void native_function_end(const char* functionName, unsigned long long int functionID, unsigned long long int timestamp)
{
  xdp::VPDatabase* db = xdp::nativePluginInstance.getDatabase() ;

  uint64_t start =
    (db->getDynamicInfo()).matchingStart(static_cast<uint64_t>(functionID)) ;

  xdp::VTFEvent* event =
    new xdp::NativeAPICall(start,
                           static_cast<double>(timestamp),
                           (db->getDynamicInfo()).addString(functionName)) ;
  (db->getDynamicInfo()).addUnsortedEvent(event) ;
}
