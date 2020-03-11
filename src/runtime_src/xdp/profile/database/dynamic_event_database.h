/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef VP_DYNAMIC_EVENT_DATABASE_DOT_H
#define VP_DYNAMIC_EVENT_DATABASE_DOT_H

#include <map>
#include <mutex>
#include <vector>
#include <fstream>
#include <functional>

#include "xdp/profile/database/events/vtf_event.h"

// For the Trace results vector
#include "xclperf.h"

#include "xdp/config.h"

namespace xdp {

  // The Dynamic Database will own all VTFEvents and is responsible
  //  for cleaning up this memory.
  class VPDynamicDatabase
  {
  private:
    // For host events, we are guaranteed that all of the timestamps
    //  will come in sequential order.  For this, we can use 
    //  a simple vector.  For low overhead profiling, we can provide
    //  capability to have this be preallocated.
    std::vector<VTFEvent*> hostEvents ;

    // Every device will have its own set of events.  Since the actual
    //  hardware might shuffle the order of events we have to make sure
    //  that this set of events is ordered based on timestamp.
    //  (The void* is actually pointing at a DeviceIntf, but we don't
    //  actually read from the device here so we just use it as 
    //  an index into the map)
    std::map<void*, std::multimap<double, VTFEvent*> > deviceEvents ;

    // A unique event id for every event added to the database.
    //  It starts with 1 so we can use 0 as an indicator of NULL
    uint64_t eventId ;

    // Data structure for matching start events with end events, 
    //  as in API calls.  This will match a function ID to event IDs.
    std::map<uint64_t, uint64_t> startMap ;

    // In order to reduce memory overhead, instead of each event holding
    //  strings, each event will instead point to a unique
    //  instance of that string
    std::map<std::string, uint64_t> stringTable ;
    uint64_t stringId ;

    // Since events can be logged from multiple threads simultaneously,
    //  we have to maintain exclusivity
    std::mutex dbLock ;

    void addHostEvent(VTFEvent* event) ;
    void addDeviceEvent(void* dev, VTFEvent* event) ;
    
  public:
    XDP_EXPORT VPDynamicDatabase() ;
    XDP_EXPORT ~VPDynamicDatabase() ;

    // Add an event in sorted order in the database
    XDP_EXPORT void addEvent(VTFEvent* event) ;
    XDP_EXPORT void addDeviceEvents(void* dev, xclTraceResultsVector& trace) ;

    // For API events, find the event id of the start event for an end event
    XDP_EXPORT void markStart(uint64_t functionID, uint64_t eventID) ;
    XDP_EXPORT uint64_t matchingStart(uint64_t functionID) ;

    // A lookup into the string table
    XDP_EXPORT uint64_t addString(const std::string& value) ;

    // A function that iterates on the dynamic events and returns
    //  events based upon the filter passed in
    XDP_EXPORT std::vector<VTFEvent*> filterEvents(std::function<bool(VTFEvent*)> filter);

    // Functions that dump large portions of the database
    XDP_EXPORT void dumpStringTable(std::ofstream& fout) ;
  } ;
  
}

#endif
