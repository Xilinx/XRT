/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include "xdp/profile/plugin/user/user_cb.h"
#include "xdp/profile/plugin/user/user_plugin.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/events/user_events.h"

#include "core/common/time.h"

namespace xdp {

  static UserEventsPlugin userEventsPluginInstance ;

  static void user_event_start_cb(unsigned int functionID,
				  const char* label,
				  const char* tooltip) 
  {
    double timestamp = xrt_core::time_ns() ;
    VPDatabase* db = userEventsPluginInstance.getDatabase() ;

    const char* labelStr   = (label == nullptr)   ? "" : label ;
    const char* tooltipStr = (tooltip == nullptr) ? "" : tooltip ;

    VTFEvent* event = new UserRange(0, 
				    timestamp, 
				    true, // isStart
				    (db->getDynamicInfo()).addString(labelStr),
				    (db->getDynamicInfo()).addString(tooltipStr)) ;
    (db->getDynamicInfo()).addEvent(event) ;
    (db->getDynamicInfo()).markStart(functionID, event->getEventId()) ;
  }

  static void user_event_end_cb(unsigned int functionID)
  {
    double timestamp = xrt_core::time_ns() ;
    VPDatabase* db = userEventsPluginInstance.getDatabase() ;

    uint64_t start = (db->getDynamicInfo()).matchingStart(functionID) ;
    VTFEvent* event = new UserRange(start, 
				    timestamp, 
				    false, // isStart
				    0,
				    0) ;

    (db->getDynamicInfo()).addEvent(event) ;
  }

  static void user_event_happened_cb(const char* label)
  {
    double timestamp = xrt_core::time_ns() ;
    VPDatabase* db = userEventsPluginInstance.getDatabase() ;

    uint64_t l = 0 ;

    if (label != nullptr)
      l = (db->getDynamicInfo()).addString(label) ;

    VTFEvent* event = new UserMarker(0, timestamp, l) ;
    (db->getDynamicInfo()).addEvent(event) ;
  }

} // end namespace xdp

extern "C" 
void user_event_start_cb(unsigned int functionID, 
			 const char* label, 
			 const char* tooltip) 
{
  xdp::user_event_start_cb(functionID, label, tooltip) ;
}

extern "C"
void user_event_end_cb(unsigned int functionID)
{
  xdp::user_event_end_cb(functionID) ;
}
    
extern "C"
void user_event_happened_cb(const char* label)
{
  xdp::user_event_happened_cb(label) ;
}
