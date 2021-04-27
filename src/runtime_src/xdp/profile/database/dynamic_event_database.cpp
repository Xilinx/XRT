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

#include "core/common/time.h"

#include <iostream>

namespace xdp {
  
  VPDynamicDatabase::VPDynamicDatabase(VPDatabase* d) :
    db(d), eventId(1), stringId(1)
  {
    // For low overhead profiling, we will reserve space for 
    //  a set number of events.  This won't change HAL or OpenCL 
    //  profiling either.
    //hostEvents.reserve(100);
  }

  VPDynamicDatabase::~VPDynamicDatabase()
  {
    {
      std::lock_guard<std::mutex> lock(aieLock) ;
      for(auto mapEntry : aieTraceData) {
        for(auto info : mapEntry.second) {
          delete info;
        }
        mapEntry.second.clear();
      }
      aieTraceData.clear();
    }

    {
      std::lock_guard<std::mutex> lock(hostEventsLock) ;
      for (auto event : hostEvents) {
      delete event.second;
      }
    }

    {
      std::lock_guard<std::mutex> lock(deviceEventsLock) ;
      for (auto device : deviceEvents) {
        for (auto multimapEntry : device.second) {
              delete multimapEntry.second;
        }
        device.second.clear();
      }
    }
  }

  void VPDynamicDatabase::markXclbinEnd(uint64_t deviceId)
  {
    addDeviceEvent(deviceId, new XclbinEnd(0, (double)(xrt_core::time_ns())/1e6, 0, 0)) ;
  }

  void VPDynamicDatabase::addHostEvent(VTFEvent* event)
  {
    std::lock_guard<std::mutex> lock(hostEventsLock) ;

    event->setEventId(eventId++) ;
    hostEvents.emplace(event->getTimestamp(), event) ;
    //hostEvents.push_back(event) ;
  }

  void VPDynamicDatabase::addUnsortedEvent(VTFEvent* event)
  {
    std::lock_guard<std::mutex> lock(unsortedEventsLock) ;
    event->setEventId(eventId++) ;

    unsortedHostEvents.push_back(event) ;
  }

  void VPDynamicDatabase::addDeviceEvent(uint64_t deviceId, VTFEvent* event)
  {
    std::lock_guard<std::mutex> lock(deviceEventsLock) ;

    event->setEventId(eventId++) ;
    deviceEvents[deviceId].emplace(event->getTimestamp(), event) ;
  }

  void VPDynamicDatabase::addEvent(VTFEvent* event)
  {
    if (event == nullptr) return ;

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
    std::lock_guard<std::mutex> lock(deviceLock);
    deviceEventStartMap[traceID].push_back(event) ;
  }

  VTFEvent* VPDynamicDatabase::matchingDeviceEventStart(uint64_t traceID, VTFEventType type)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;
    VTFEvent* startEvent = nullptr;
    auto& lst = deviceEventStartMap[traceID];
    if (!lst.empty())
    {
      for (auto itr = lst.begin() ; itr != lst.end(); ++itr) {
        if((*itr)->getEventType() == type) {
          startEvent = (*itr);
          lst.erase(itr);
          break;
        }
      }
    }
    return startEvent;
  }

  bool VPDynamicDatabase::hasMatchingDeviceEventStart(uint64_t traceID,
                                                      VTFEventType type)
  {
    std::lock_guard<std::mutex> lock(deviceLock) ;
    if (deviceEventStartMap.find(traceID) == deviceEventStartMap.end())
      return false ;
    auto& lst = deviceEventStartMap[traceID] ;
    for (auto e: lst) {
      if (e->getEventType() == type)
        return true ;
    }
    return false ;
  }

  void VPDynamicDatabase::markStart(uint64_t functionID, uint64_t eventID)
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    startMap[functionID] = eventID ;
  }

  uint64_t VPDynamicDatabase::matchingStart(uint64_t functionID)
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    if (startMap.find(functionID) != startMap.end())
    {
      uint64_t value = startMap[functionID] ;
      startMap.erase(functionID) ;
      return value ;
    }
    return 0 ;
  }

  void VPDynamicDatabase::markXRTUIDStart(uint64_t uid, uint64_t eventID)
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    uidMap[uid] = eventID ;
  }

  uint64_t VPDynamicDatabase::matchingXRTUIDStart(uint64_t uid)
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    if (uidMap.find(uid) != uidMap.end()) {
      uint64_t value = uidMap[uid] ;
      uidMap.erase(uid) ;
      return value ;
    }
    return 0 ;
  }

  void VPDynamicDatabase::markRange(uint64_t functionID,
                                    std::pair<const char*, const char*> desc,
                                    uint64_t startTimestamp)
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    std::tuple<const char*, const char*, uint64_t> triple;
    std::get<0>(triple) = desc.first ;
    std::get<1>(triple) = desc.second ;
    std::get<2>(triple) = startTimestamp ;
    userMap[functionID] = triple ;
  }

  std::tuple<const char*, const char*, uint64_t>
  VPDynamicDatabase::matchingRange(uint64_t functionID)
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    std::tuple<const char*, const char*, uint64_t> value ;
    std::get<0>(value) = "" ;
    std::get<1>(value) = "" ;
    std::get<2>(value) = 0 ;
    if (userMap.find(functionID) != userMap.end()) {
      value = userMap[functionID] ;
      userMap.erase(functionID) ;
    }
    return value ;
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
    std::vector<VTFEvent*> collected ;

    // For now, go through both host events and device events.
    {
      std::lock_guard<std::mutex> lock(hostEventsLock) ;
      for (auto e : hostEvents) {
        if (filter(e.second)) collected.push_back(e.second) ;
      }
    }

    {
      std::lock_guard<std::mutex> lock(deviceEventsLock) ;
      for (auto dev : deviceEvents) {
        for (auto multiMapEntry : dev.second) {
          if (filter(multiMapEntry.second)) collected.push_back(multiMapEntry.second) ;
        }
      }
    }

    return collected ;
  }

  std::vector<VTFEvent*> VPDynamicDatabase::filterHostEvents(std::function<bool(VTFEvent*)> filter)
  {
    std::lock_guard<std::mutex> lock(hostEventsLock) ;
    std::vector<VTFEvent*> collected ;

    for (auto e : hostEvents)
    {
      if (filter(e.second)) collected.push_back(e.second) ;
    }
    return collected ;
  }

  std::vector<std::unique_ptr<VTFEvent>> VPDynamicDatabase::filterEraseHostEvents(std::function<bool(VTFEvent*)> filter)
  {
    std::lock_guard<std::mutex> lock(hostEventsLock) ;
    std::vector<std::unique_ptr<VTFEvent>> collected ;

    for (auto it=hostEvents.begin(); it!=hostEvents.end();) {
      if (filter(it->second)) {
        collected.emplace_back(it->second);
        it = hostEvents.erase(it);
      } else {
        ++it;
      }
    }
    return collected ;
  }

  std::vector<VTFEvent*>
  VPDynamicDatabase::
  filterEraseUnsortedHostEvents(std::function<bool(VTFEvent*)> filter)
  {
    std::lock_guard<std::mutex> lock(unsortedEventsLock);
    std::vector<VTFEvent*> collected ;

    for (auto it=unsortedHostEvents.begin(); it!=unsortedHostEvents.end();){
      if (filter(*it)) {
        collected.emplace_back(*it);
        it = unsortedHostEvents.erase(it);
      } else {
        ++it ;
      }
    }
    return collected ;
  }

  std::vector<VTFEvent*> VPDynamicDatabase::getHostEvents()
  {
    std::lock_guard<std::mutex> lock(hostEventsLock) ;
    std::vector<VTFEvent*> events;
    for(auto e : hostEvents) {
      events.push_back(e.second);
    }
    return events;
  }

  bool VPDynamicDatabase::hostEventsExist(std::function<bool(VTFEvent*)> filter)
  {
    std::lock_guard<std::mutex> lock(hostEventsLock) ;
    for (auto it=hostEvents.begin(); it!=hostEvents.end(); it++) {
      if (filter(it->second))
        return true;
    }
    return false;
  }

  bool VPDynamicDatabase::deviceEventsExist(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceEventsLock) ;
    return !(deviceEvents[deviceId].empty());
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

  std::vector<std::unique_ptr<VTFEvent>> VPDynamicDatabase::getEraseDeviceEvents(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceEventsLock) ;
    std::vector<std::unique_ptr<VTFEvent>> events;
    if(deviceEvents.find(deviceId) == deviceEvents.end()) {
      return events;
    }
    auto& mmap = deviceEvents[deviceId];
    for (auto it=mmap.begin(); it!=mmap.end();) {
      events.emplace_back(it->second);
      it = mmap.erase(it);
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

  void VPDynamicDatabase::setCounterResults(const uint64_t deviceId,
                                            xrt_core::uuid uuid,
                                            xclCounterResults& values)
  {
    std::lock_guard<std::mutex> lock(ctrLock) ;
    std::pair<uint64_t, xrt_core::uuid> index = std::make_pair(deviceId, uuid) ;

    deviceCounters[index] = values ;
  }

  xclCounterResults VPDynamicDatabase::getCounterResults(uint64_t deviceId,
                                                         xrt_core::uuid uuid)
  {
    std::lock_guard<std::mutex> lock(ctrLock) ;
    std::pair<uint64_t, xrt_core::uuid> index = std::make_pair(deviceId, uuid) ;

    return deviceCounters[index] ;
  }

  void VPDynamicDatabase::addOpenCLMapping(uint64_t openclID,
                                           uint64_t eventID,
                                           uint64_t startID)
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    openclEventMap[openclID] = std::make_pair(eventID, startID) ;
  }

  std::pair<uint64_t, uint64_t>
  VPDynamicDatabase::lookupOpenCLMapping(uint64_t openclID)
  {
    if (openclEventMap.find(openclID) == openclEventMap.end())
      return std::make_pair(0, 0) ;
    return openclEventMap[openclID] ;
  }

  void VPDynamicDatabase::addDependencies(uint64_t eventID,
                                          const std::vector<uint64_t>& openclIDs)
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    dependencyMap[eventID] = openclIDs ;
  }

  void VPDynamicDatabase::addDependency(uint64_t id, uint64_t dependency)
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    if (dependencyMap.find(id) == dependencyMap.end())
    {
      std::vector<uint64_t> blank ;
      dependencyMap[id] = blank ;
    }
    (dependencyMap[id]).push_back(dependency) ;
  }

  std::map<uint64_t, std::vector<uint64_t>>
  VPDynamicDatabase::getDependencyMap()
  {
    std::lock_guard<std::mutex> lock(hostLock) ;
    return dependencyMap ;
  }

  void VPDynamicDatabase::addAIETraceData(uint64_t deviceId,
                             uint64_t strmIndex, void* buffer, uint64_t bufferSz) 
  {
    std::lock_guard<std::mutex> lock(aieLock);

    if(aieTraceData.find(deviceId) == aieTraceData.end()) {
      AIETraceDataVector newDataVector;
      aieTraceData[deviceId] = newDataVector;	// copy
      aieTraceData[deviceId].resize((db->getStaticInfo()).getNumAIETraceStream(deviceId));
    }
    if(nullptr == aieTraceData[deviceId][strmIndex]) {
      aieTraceData[deviceId][strmIndex] = new AIETraceDataType;
    }
    aieTraceData[deviceId][strmIndex]->buffer.push_back(buffer);
    aieTraceData[deviceId][strmIndex]->bufferSz.push_back(bufferSz);
#if 0
    aieTraceData[deviceId][strmIndex] = new AIETraceDataType;
    aieTraceData[deviceId][strmIndex]->buffer = buffer;
    aieTraceData[deviceId][strmIndex]->bufferSz = bufferSz;
#endif
  }

  AIETraceDataType* VPDynamicDatabase::getAIETraceData(uint64_t deviceId, uint64_t strmIndex)
  {
    std::lock_guard<std::mutex> lock(aieLock) ;

    if(aieTraceData.find(deviceId) == aieTraceData.end()) {
        return nullptr;
    }
    auto aieTraceDataEntry = aieTraceData[deviceId];
    if(aieTraceData[deviceId].size() == 0 || aieTraceDataEntry[strmIndex] == nullptr) {
        return nullptr;
    }
    return aieTraceDataEntry[strmIndex];
  }

  void VPDynamicDatabase::addPowerSample(uint64_t deviceId, double timestamp,
          const std::vector<uint64_t>& values)
  {
    std::lock_guard<std::mutex> lock(powerLock) ;

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
    std::lock_guard<std::mutex> lock(powerLock) ;

    return powerSamples[deviceId] ;
  }

  void VPDynamicDatabase::addAIESample(uint64_t deviceId, double timestamp,
          const std::vector<uint64_t>& values)
  {
    std::lock_guard<std::mutex> lock(aieLock) ;

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
    std::lock_guard<std::mutex> lock(aieLock) ;

    if (aieSamples.find(deviceId) == aieSamples.end()) {
      std::vector<CounterSample> empty;
      aieSamples[deviceId] = empty;
    }

    return aieSamples[deviceId] ;
  }

  void VPDynamicDatabase::addNOCSample(uint64_t deviceId, double timestamp,
          std::string name, const std::vector<uint64_t>& values)
  {
    std::lock_guard<std::mutex> lock(nocLock) ;

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
    std::lock_guard<std::mutex> lock(nocLock) ;

    return nocSamples[deviceId] ;
  }

  VPDynamicDatabase::CounterNames
  VPDynamicDatabase::getNOCNames(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(nocLock) ;

    return nocNames[deviceId] ;
  }
}
