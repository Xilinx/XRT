/**
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

#ifndef HOST_DB_DOT_H
#define HOST_DB_DOT_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "xdp/config.h"
#include "xdp/profile/database/dynamic_info/dependency_manager.h"
#include "xdp/profile/database/dynamic_info/mark.h"
#include "xdp/profile/database/dynamic_info/types.h"

namespace xdp {

  // Forward declarations
  class VTFEvent;

  // The HostDB contains all the dynamic event information related
  // to the different host tracing and anything higher level (like user events)
  class HostDB
  {
  private:
    static constexpr uint64_t eventThreshold = 10000000;

    // Before all events are printed in a CSV, they have to be sorted.
    // The multimap sorts them as we create and insert them.
    std::multimap<double, VTFEvent*> sortedEvents;

    // For host events that will be sorted later (when printed), we
    // can store them away in a simple vector
    std::vector<VTFEvent*> unsortedEvents;

    // This object keeps track of matching start events with end events
    APIMatch<uint64_t, uint64_t> eventStarts;

    // This object keeps track of matching start and end user ranges
    APIMatch<uint64_t, UserRangeInfo> userStarts;

    // This object keeps track of matching start and end host events
    // that do not have unique event IDs.  For example, we want to
    // mark the start and end of different parts of the OpenCL queue,
    // but all use the same event.  Here, we use a unique ID different
    // from the event IDs and the user IDs.
    APIMatch<uint64_t, uint64_t> uidStarts;

    // This object keeps track of matching start events with end events
    // for situations where we have one callback creating two database events.
    APIMatch<uint64_t, EventPair> eventPairStarts;

    // Different host layers can have dependencies between events
    DependencyManager openclDependencies;

    std::mutex sortedLock; // Protects the "sortedEvents" multimap
    std::mutex unsortedLock; // Protects the "unsortedEvents" vector

  public:
    HostDB() = default;
    XDP_CORE_EXPORT ~HostDB();

    // Functions to add host events to the database
    void addSortedEvent(VTFEvent* event);
    void addUnsortedEvent(VTFEvent* event);

    // A function to check the sorted events to see if any events that
    // fit the filter exist are currently stored in the database.
    bool sortedEventsExist(std::function<bool (VTFEvent*)>& filter);

    // A function that goes through all the sorted events and create
    // a vector of copies of the events that fit the filter
    std::vector<VTFEvent*>
    filterSortedEvents(std::function<bool (VTFEvent*)>& filter);

    // A function that goes through all the unsorted events and create
    // a vector of copies of the events that fit the filter
    std::vector<VTFEvent*>
    filterUnsortedEvents(std::function<bool (VTFEvent*)>& filter);

    // A function that goes through all the sorted events and creates
    // a vector of the events that fit the filter.  This transfers
    // ownership of the events to the caller.
    std::vector<std::unique_ptr<VTFEvent>>
    moveSortedEvents(std::function<bool (VTFEvent*)>& filter);

    // A function that goes through all the unsorted events and
    // creates a vector of the events that fit the filter.  This
    // transfers ownership of the events to the caller.
    // Not a unique pointer because it needs to be sorted later.
    std::vector<VTFEvent*>
    moveUnsortedEvents(std::function<bool (VTFEvent*)>& filter);

    // Functions for matching start events with end events
    inline void registerStart(uint64_t functionId, uint64_t eventId)
    { eventStarts.registerStart(functionId, eventId); }

    inline uint64_t lookupStart(uint64_t functionId)
    { return eventStarts.lookupStart(functionId); }

    inline void registerUserStart(uint64_t functionId, const UserRangeInfo& s)
    { userStarts.registerStart(functionId, s);  }

    inline UserRangeInfo lookupUserStart(uint64_t functionId)
    { return userStarts.lookupStart(functionId);  }

    inline void registerUIDStart(uint64_t uid, uint64_t eventId)
    { uidStarts.registerStart(uid, eventId); }

    inline uint64_t matchingXRTUIDStart(uint64_t uid)
    { return uidStarts.lookupStart(uid); }

    inline void registerEventPairStart(uint64_t functionId, const EventPair& events)
    { eventPairStarts.registerStart(functionId, events); }

    inline EventPair matchingEventPairStart(uint64_t functionId)
    { return eventPairStarts.lookupStart(functionId); }

    // Functions for handling dependencies
    inline void addOpenCLMapping(uint64_t openclId, uint64_t endXDPEventId,
                                 uint64_t startXDPEventId)
    { openclDependencies.addOpenCLMapping(openclId, endXDPEventId,
                                          startXDPEventId);  }

    inline std::pair<uint64_t, uint64_t> lookupOpenCLMapping(uint64_t openclId)
    { return openclDependencies.lookupOpenCLMapping(openclId); }

    inline void addDependency(uint64_t id, uint64_t dependency)
    { openclDependencies.addDependency(id, dependency);  }

    inline std::map<uint64_t, std::vector<uint64_t>> copyDependencyMap()
    { return openclDependencies.copyDependencyMap(); }

  };

};

#endif
