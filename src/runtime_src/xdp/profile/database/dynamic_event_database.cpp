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

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/dynamic_event_database.h"
#include "xdp/profile/database/events/device_events.h"

namespace xdp {
  
  VPDynamicDatabase::VPDynamicDatabase(VPDatabase* d) :
    db(d), eventId(1), stringId(1)
  {
    // For low overhead profiling, we will reserve space for 
    //  a set number of events.  This won't change HAL or OpenCL 
    //  profiling either.
    hostEvents.reserve(100);
  }

  VPDynamicDatabase::~VPDynamicDatabase()
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    for (auto event : hostEvents) {
      delete event;
    }

    for (auto device : deviceEvents) {
      for (auto multimapEntry : device.second) {
	    delete multimapEntry.second;
      }
      device.second.clear();
    }
  }

  void VPDynamicDatabase::addHostEvent(VTFEvent* event)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    hostEvents.push_back(event) ;
  }

  void VPDynamicDatabase::addDeviceEvent(uint64_t deviceId, VTFEvent* event)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    deviceEvents[deviceId].emplace(event->getTimestamp(), event) ;
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

  void VPDynamicDatabase::markDeviceEventStart(uint64_t traceID, VTFEvent* event)
  {
    std::lock_guard<std::mutex> lock(dbLock);
    deviceEventStartMap[traceID].push_back(event) ;
  }

  VTFEvent* VPDynamicDatabase::matchingDeviceEventStart(uint64_t traceID)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;
    if (deviceEventStartMap.find(traceID) != deviceEventStartMap.end() && !deviceEventStartMap[traceID].empty())
    {
      VTFEvent* startEvent = deviceEventStartMap[traceID].front();
      deviceEventStartMap[traceID].pop_front();
      return startEvent ;
    }
    return nullptr;
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
      for (auto multiMapEntry : dev.second)
      {
	if (filter(multiMapEntry.second)) collected.push_back(multiMapEntry.second) ;
      }
    }

    return collected ;
  }

  std::vector<VTFEvent*> VPDynamicDatabase::getHostEvents()
  {
    std::vector<VTFEvent*> events;
    for(auto e : hostEvents) {
      events.push_back(e);
    }
    return events;
  }

  std::vector<VTFEvent*> VPDynamicDatabase::getDeviceEvents(uint64_t deviceId)
  {
    std::vector<VTFEvent*> events;
    if(deviceEvents.find(deviceId) == deviceEvents.end()) {
      return events;
    }
    for(auto multiMapEntry : deviceEvents[deviceId]) {
      events.push_back(multiMapEntry.second);
    }
    return events;
  }

  void VPDynamicDatabase::dumpStringTable(std::ofstream& fout)
  {
    // Windows compilation fails unless c_str() is used
    for (auto s : stringTable)
    {
      fout << s.second << "," << s.first.c_str() << std::endl ;
    }
  }

  void VPDynamicDatabase::addAIETraceData(uint64_t deviceId,
                             uint64_t strmIndex, void* buffer, uint64_t bufferSz) 
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    if(aieTraceData.find(deviceId) == aieTraceData.end()) {
      AIETraceDataVector newDataVector;
      aieTraceData[deviceId] = newDataVector;	// copy
      aieTraceData[deviceId].resize((db->getStaticInfo()).getNumAIETraceStream(deviceId));
    }
    auto aieTraceDataEntry = aieTraceData[deviceId];
    aieTraceDataEntry[strmIndex] = std::make_pair(buffer, bufferSz);
  }

  AIETraceDataType VPDynamicDatabase::getAIETraceData(uint64_t deviceId, uint64_t strmIndex)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    auto aieTraceDataEntry = aieTraceData[deviceId];
    return aieTraceDataEntry[strmIndex];
  }

  void VPDynamicDatabase::addPowerSample(uint64_t deviceId, double timestamp,
          const std::vector<uint64_t>& values)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    if (powerSamples.find(deviceId) == powerSamples.end())
    {
      std::vector<CounterSample> blank ;
      powerSamples[deviceId] = blank ;
    }

    powerSamples[deviceId].push_back(std::make_pair(timestamp, values)) ;
  }

  std::vector<VPDynamicDatabase::CounterSample>
  VPDynamicDatabase::getPowerSamples(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    return powerSamples[deviceId] ;
  }

  void VPDynamicDatabase::addAIESample(uint64_t deviceId, double timestamp,
          const std::vector<uint64_t>& values)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    if (aieSamples.find(deviceId) == aieSamples.end())
    {
      std::vector<CounterSample> blank ;
      aieSamples[deviceId] = blank ;
    }

    aieSamples[deviceId].push_back(std::make_pair(timestamp, values)) ;
  }

  std::vector<VPDynamicDatabase::CounterSample>
  VPDynamicDatabase::getAIESamples(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    return aieSamples[deviceId] ;
  }

  void VPDynamicDatabase::addNOCSample(uint64_t deviceId, double timestamp,
          std::string name, const std::vector<uint64_t>& values)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    // Store name
    if (nocNames.find(deviceId) == nocNames.end())
    {
      CounterNames blank ;
      nocNames[deviceId] = blank ;
    }

    nocNames[deviceId][timestamp] = name ;

    // Store vector of values
    if (nocSamples.find(deviceId) == nocSamples.end())
    {
      std::vector<CounterSample> blank ;
      nocSamples[deviceId] = blank ;
    }

    nocSamples[deviceId].push_back(std::make_pair(timestamp, values)) ;
  }

  std::vector<VPDynamicDatabase::CounterSample>
  VPDynamicDatabase::getNOCSamples(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    return nocSamples[deviceId] ;
  }

  VPDynamicDatabase::CounterNames
  VPDynamicDatabase::getNOCNames(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(dbLock) ;

    return nocNames[deviceId] ;
  }
}
