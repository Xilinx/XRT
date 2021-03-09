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

  static void native_function_start(const char* functionName, uint64_t functionID)
  {
    uint64_t timestamp = xrt_core::time_ns() ;
    VPDatabase* db = nativePluginInstance.getDatabase() ;

    VTFEvent* event = new NativeAPICall(0,
					static_cast<double>(timestamp),
					(db->getDynamicInfo()).addString(functionName)) ;
    (db->getDynamicInfo()).addEvent(event);
    (db->getDynamicInfo()).markStart(functionID, event->getEventId()) ;
  }

  static void native_function_end(const char* functionName, uint64_t functionID)
  {
    uint64_t timestamp = xrt_core::time_ns() ;
    VPDatabase* db = nativePluginInstance.getDatabase() ;

    uint64_t start = (db->getDynamicInfo()).matchingStart(functionID) ;

    VTFEvent* event = new NativeAPICall(start,
					static_cast<double>(timestamp),
					(db->getDynamicInfo()).addString(functionName)) ;
    (db->getDynamicInfo()).addEvent(event) ;
  }

} // end namespace xdp

extern "C"
void native_function_start(const char* functionName, unsigned long long int functionID)
{
  xdp::native_function_start(functionName, static_cast<uint64_t>(functionID)) ;
}

extern "C"
void native_function_end(const char* functionName, unsigned long long int functionID)
{
  xdp::native_function_end(functionName, static_cast<uint64_t>(functionID)) ;
}
