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

#include "xdp/profile/database/dynamic_info/host_db.h"
#include "xdp/profile/database/events/vtf_event.h"

namespace xdp {

  HostDB::~HostDB()
  {
    // Delete sorted events still in the database and not moved
    {
      std::lock_guard<std::mutex> lock(sortedLock);
      for (auto& iter : sortedEvents) {
        auto event = iter.second;
        delete event;
      }
    }
    // Delete unsorted events still in the database and not moved
    {
      std::lock_guard<std::mutex> lock(unsortedLock);
      for (auto event : unsortedEvents)
        delete event;
    }
  }

  void HostDB::addSortedEvent(VTFEvent* event)
  {
    if (event == nullptr)
      return;

    std::lock_guard<std::mutex> lock(sortedLock);
    sortedEvents.emplace(event->getTimestamp(), event);
  }

  void HostDB::addUnsortedEvent(VTFEvent* event)
  {
    if (event == nullptr)
      return;

    std::lock_guard<std::mutex> lock(unsortedLock);
    unsortedEvents.push_back(event);
  }

  bool HostDB::sortedEventsExist(std::function<bool (VTFEvent*)>& filter)
  {
    std::lock_guard<std::mutex> lock(sortedLock);
    for (auto& iter : sortedEvents) {
      auto event = iter.second;
      if (filter(event))
        return true;
    }
    return false;
  }

  std::vector<VTFEvent*>
  HostDB::filterSortedEvents(std::function<bool (VTFEvent*)>& filter)
  {
    std::lock_guard<std::mutex> lock(sortedLock);

    std::vector<VTFEvent*> collected;
    for (auto& iter : sortedEvents) {
      auto event = iter.second;
      if (filter(event))
        collected.push_back(event);
    }
    return collected;
  }

  std::vector<VTFEvent*>
  HostDB::filterUnsortedEvents(std::function<bool (VTFEvent*)>& filter)
  {
    std::lock_guard<std::mutex> lock(unsortedLock);

    std::vector<VTFEvent*> collected;
    for (auto event : unsortedEvents) {
      if (filter(event))
        collected.push_back(event);
    }
    return collected;
  }

  std::vector<std::unique_ptr<VTFEvent>>
  HostDB::moveSortedEvents(std::function<bool (VTFEvent*)>& filter)
  {
    std::lock_guard<std::mutex> lock(sortedLock);

    std::vector<std::unique_ptr<VTFEvent>> collected;

    for (auto iter = sortedEvents.begin(); iter != sortedEvents.end(); ) {
      auto event = (*iter).second;
      if (filter(event)) {
        collected.emplace_back(event);
        iter = sortedEvents.erase(iter);
      }
      else
        ++iter;
    }
    return collected;
  }

  std::vector<VTFEvent*>
  HostDB::moveUnsortedEvents(std::function<bool (VTFEvent*)>& filter)
  {
    std::lock_guard<std::mutex> lock(unsortedLock);

    std::vector<VTFEvent*> collected;

    for (auto iter = unsortedEvents.begin();
         iter != unsortedEvents.end();
         /* Intentionally blank*/) {
      auto event = *iter;
      if (filter(event)) {
        collected.emplace_back(event);
        iter = unsortedEvents.erase(iter);
      }
      else
        ++iter;
    }
    return collected;
  }

} // end namespace xdp
