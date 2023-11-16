/**
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

#define XDP_CORE_SOURCE

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/dynamic_info/pl_db.h"
#include "xdp/profile/database/events/vtf_event.h"

namespace xdp {

  void PLDB::addEvent(VTFEvent* event)
  {
    if (event == nullptr)
      return;

    bool overLimit = false;
    {
      std::lock_guard<std::mutex> lock(eventLock);
      events.emplace(event->getTimestamp(), event);
      if (events.size() > eventThreshold)
        overLimit = true;
    }
    if (overLimit)
      VPDatabase::Instance()->broadcast(VPDatabase::DUMP_TRACE);
  }

  bool PLDB::eventsExist()
  {
    std::lock_guard<std::mutex> lock(eventLock);
    return !events.empty();
  }

  std::vector<std::unique_ptr<VTFEvent>> PLDB::moveEvents()
  {
    std::lock_guard<std::mutex> lock(eventLock);

    std::vector<std::unique_ptr<VTFEvent>> collected;
    for (auto iter = events.begin(); iter != events.end(); /* */) {
      auto event = iter->second;
      collected.emplace_back(event);
      iter = events.erase(iter);
    }
    return collected;
  }

  void PLDB::markStart(uint64_t monitorId, const DeviceEventInfo& info)
  {
    std::lock_guard<std::mutex> lock(startLock);

    startEvents[monitorId].push_back(info);
  }

  DeviceEventInfo PLDB::findMatchingStart(uint64_t monitorId, VTFEventType type)
  {
    std::lock_guard<std::mutex> lock(startLock);
    DeviceEventInfo eventInfo;
    eventInfo.type = UNKNOWN_EVENT;
    eventInfo.eventID = 0;
    eventInfo.hostTimestamp = 0.0;
    eventInfo.deviceTimestamp = 0;
    auto& lst = startEvents[monitorId];

    for (auto iter = lst.begin(); iter != lst.end(); ++iter) {
      if ((*iter).type == type) {
        eventInfo = (*iter);
        lst.erase(iter);
        break;
      }
    }

    return eventInfo;
  }

  bool PLDB::hasMatchingStart(uint64_t monitorId, VTFEventType type)
  {
    std::lock_guard<std::mutex> lock(startLock);
    auto& lst = startEvents[monitorId];
    for (auto& e : lst) {
      if (e.type == type)
        return true;
    }
    return false;
  }

  void PLDB::setPLTraceBufferFull(bool val)
  {
    std::lock_guard<std::mutex> lock(fullLock);
    plTraceBufferFull = val;
  }

  bool PLDB::isPLTraceBufferFull()
  {
    // Protect the stored value and return a copy
    std::lock_guard<std::mutex> lock(fullLock);
    return plTraceBufferFull;
  }

  void PLDB::setPLCounterResults(xrt_core::uuid uuid, CounterResults& values)
  {
    std::lock_guard<std::mutex> lock(counterLock);
    plCounters[uuid] = values;
  }

  CounterResults PLDB::getPLCounterResults(xrt_core::uuid uuid)
  {
    // Protect the map from multithreaded access and return a copy
    std::lock_guard<std::mutex> lock(counterLock);
    return plCounters[uuid];
  }

} // end namespace xdp
