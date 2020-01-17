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

#define XDP_SOURCE

#include "xdp/profile/database/dynamic_event_database.h"

namespace xdp {
  
  VPDynamicDatabase::VPDynamicDatabase() : 
    eventId(1), stringId(1)
  {
    // For low overhead profiling, we will reserve space for 
    //  a set number of events.  This won't change HAL or OpenCL 
    //  profiling either.
    hostEvents.reserve(100);
  }

  VPDynamicDatabase::~VPDynamicDatabase()
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    for (auto event : hostEvents)
    {
      delete event ;
    }

    for (auto device : deviceEvents)
    {
      for (auto element : device.second)
      {
	delete element.second ;
      }
    }
  }

  void VPDynamicDatabase::addHostEvent(VTFEvent* event)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    hostEvents.push_back(event) ;
  }

  void VPDynamicDatabase::addDeviceEvent(void* dev, VTFEvent* event)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    deviceEvents[dev].emplace(event->getTimestamp(), event) ;
  }

  void VPDynamicDatabase::addEvent(VTFEvent* event)
  {
    if (event == nullptr) return ;
    event->setEventId(eventId++) ;

    if (event->isDeviceEvent())
    {
      addDeviceEvent(event->getDevice(), event) ;
    }
    else
    {
      addHostEvent(event) ;
    }
  }

  void VPDynamicDatabase::addDeviceEvents(void* /*dev*/, 
					   xclTraceResultsVector& /*trace*/)
  {
    // TBD
  }

  void VPDynamicDatabase::markStart(uint64_t functionID, uint64_t eventID)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    startMap[functionID] = eventID ;
  }

  uint64_t VPDynamicDatabase::matchingStart(uint64_t functionID)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    if (startMap.find(functionID) != startMap.end())
    {
      uint64_t value = startMap[functionID] ;
      startMap.erase(functionID) ;
      return value ;
    }
    return 0 ;
  }

  uint64_t VPDynamicDatabase::addString(const std::string& value)
  {
    if (stringTable.find(value) == stringTable.end())
    {
      stringTable[value] = stringId++ ;
    }
    return stringTable[value] ;
  }

  // This needs to be sped up significantly.
  std::vector<VTFEvent*> VPDynamicDatabase::filterEvents(std::function<bool(VTFEvent*)> filter)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    std::vector<VTFEvent*> collected ;

    // For now, go through both host events and device events.
    for (auto e : hostEvents)
    {
      if (filter(e)) collected.push_back(e) ;
    }

    for (auto dev : deviceEvents)
    {
      for (auto e : dev.second)
      {
	if (filter(e.second)) collected.push_back(e.second) ;
      }
    }

    return collected ;
  }

  void VPDynamicDatabase::dumpStringTable(std::ofstream& fout)
  {
    // Windows compilation fails unless c_str() is used
    for (auto s : stringTable)
    {
      fout << s.second << "," << s.first.c_str() << std::endl ;
    }
  }

}
