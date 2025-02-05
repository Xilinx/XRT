/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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
#include "xdp/profile/database/dynamic_event_database.h"
#include "xdp/profile/database/events/device_events.h"

#include "core/common/time.h"

#include <iostream>

namespace xdp {

  VPDynamicDatabase::VPDynamicDatabase(VPDatabase* d) :
    db(d), eventId(1)
  {
    host = std::make_unique<HostDB>();
  }

  // For designs that load multiple xclbins, we add an event into the database
  // to mark when one xclbin is cleaned out and a new one is loaded.
  void VPDynamicDatabase::markXclbinEnd(uint64_t deviceId)
  {
    auto endEvent =
      new XclbinEnd(0, static_cast<double>(xrt_core::time_ns())/1e6, 0, 0);
    addDeviceEvent(deviceId, endEvent);
  }

  void VPDynamicDatabase::issueId(VTFEvent* event)
  {
    if (event == nullptr)
      return;
    event->setEventId(eventId++);
  }

  // This function is only called internally once an ID has been issued
  // to the event.
  void VPDynamicDatabase::addHostEvent(VTFEvent* event)
  {
    host->addSortedEvent(event);
  }

  // This function is called from plugins and needs to issue the ID.
  void VPDynamicDatabase::addUnsortedEvent(VTFEvent* event)
  {
    issueId(event);
    // Currently, only the host side stores unsorted events
    host->addUnsortedEvent(event);
  }

  // Lookup the device database corresponding with the device ID.  If
  // the device database does not yet exist, create it here.
  DeviceDB* VPDynamicDatabase::getDeviceDB(uint64_t deviceId)
  {
    std::lock_guard<std::mutex> lock(deviceDBLock);
    if (devices.find(deviceId) == devices.end())
      devices[deviceId] = std::make_unique<DeviceDB>();
    return devices[deviceId].get();
  }

  void VPDynamicDatabase::addDeviceEvent(uint64_t deviceId, VTFEvent* event)
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->addPLTraceEvent(event);
  }

  void VPDynamicDatabase::addEvent(VTFEvent* event)
  {
    if (event == nullptr)
      return;

    issueId(event);

    if (event->isDeviceEvent())
      addDeviceEvent(event->getDevice(), event);
    else
      addHostEvent(event);
  }

  void VPDynamicDatabase::markDeviceEventStart(uint64_t deviceId,
                                               uint64_t monitorId,
                                               DeviceEventInfo& info)
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->markStart(monitorId, info);
  }

  DeviceEventInfo
  VPDynamicDatabase::matchingDeviceEventStart(uint64_t deviceId,
                                              uint64_t monitorId,
                                              VTFEventType type)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->findMatchingStart(monitorId, type);
  }

  bool VPDynamicDatabase::hasMatchingDeviceEventStart(uint64_t deviceId,
                                                      uint64_t monitorId,
                                                      VTFEventType type)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->hasMatchingStart(monitorId, type);
  }

  void VPDynamicDatabase::markStart(uint64_t functionID, uint64_t eventID)
  {
    host->registerStart(functionID, eventID);
  }

  uint64_t VPDynamicDatabase::matchingStart(uint64_t functionID)
  {
    return host->lookupStart(functionID);
  }

  void VPDynamicDatabase::markXRTUIDStart(uint64_t uid, uint64_t eventID)
  {
    host->registerUIDStart(uid, eventID);
  }

  uint64_t VPDynamicDatabase::matchingXRTUIDStart(uint64_t uid)
  {
    return host->matchingXRTUIDStart(uid);
  }

  void VPDynamicDatabase::markEventPairStart(uint64_t functionId, const EventPair& events)
  {
    host->registerEventPairStart(functionId, events);
  }

  EventPair VPDynamicDatabase::matchingEventPairStart(uint64_t functionId)
  {
    return host->matchingEventPairStart(functionId);
  }

  void VPDynamicDatabase::markRange(uint64_t functionID,
                                    std::pair<const char*, const char*> desc,
                                    uint64_t startTimestamp)
  {
    UserRangeInfo triple;
    triple.label = desc.first ;
    triple.tooltip = desc.second ;
    triple.startTimestamp = startTimestamp ;
    host->registerUserStart(functionID, triple);
  }

  UserRangeInfo VPDynamicDatabase::matchingRange(uint64_t functionID)
  {
    return host->lookupUserStart(functionID);
  }

  std::vector<VTFEvent*>
  VPDynamicDatabase::copySortedHostEvents(std::function<bool(VTFEvent*)> filter)
  {
    return host->filterSortedEvents(filter);
  }

  std::vector<std::unique_ptr<VTFEvent>> VPDynamicDatabase::moveSortedHostEvents(std::function<bool(VTFEvent*)> filter)
  {
    return host->moveSortedEvents(filter);
  }

  std::vector<VTFEvent*>
  VPDynamicDatabase::
  moveUnsortedHostEvents(std::function<bool(VTFEvent*)> filter)
  {
    return host->moveUnsortedEvents(filter);
  }

  bool VPDynamicDatabase::hostEventsExist(std::function<bool(VTFEvent*)> filter)
  {
    return host->sortedEventsExist(filter);
  }

  bool VPDynamicDatabase::deviceEventsExist(uint64_t deviceId)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->eventsExist();
  }

  std::vector<std::unique_ptr<VTFEvent>>
  VPDynamicDatabase::moveDeviceEvents(uint64_t deviceId)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->moveEvents();
  }

  void VPDynamicDatabase::setCounterResults(const uint64_t deviceId,
                                            xrt_core::uuid uuid,
                                            xdp::CounterResults& values)
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->setPLCounterResults(uuid, values);
  }

  xdp::CounterResults VPDynamicDatabase::getCounterResults(uint64_t deviceId,
                                                           xrt_core::uuid uuid)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->getPLCounterResults(uuid);
  }

  void VPDynamicDatabase::addOpenCLMapping(uint64_t openclID,
                                           uint64_t eventID,
                                           uint64_t startID)
  {
    host->addOpenCLMapping(openclID, eventID, startID);
  }

  std::pair<uint64_t, uint64_t>
  VPDynamicDatabase::lookupOpenCLMapping(uint64_t openclID)
  {
    return host->lookupOpenCLMapping(openclID);
  }

  void VPDynamicDatabase::addDependency(uint64_t id, uint64_t dependency)
  {
    host->addDependency(id, dependency);
  }

  std::map<uint64_t, std::vector<uint64_t>>
  VPDynamicDatabase::getDependencyMap()
  {
    return host->copyDependencyMap();
  }

  void VPDynamicDatabase::addAIETraceData(uint64_t deviceId,
                                          uint64_t strmIndex,
                                          void* buffer,
                                          uint64_t bufferSz,
                                          bool copy)
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->addAIETraceData(strmIndex, buffer, bufferSz, copy,
                               db->getStaticInfo().getNumAIETraceStream(deviceId));
  }

  aie::TraceDataType* VPDynamicDatabase::getAIETraceData(uint64_t deviceId, uint64_t strmIndex)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->getAIETraceData(strmIndex);
  }

  void VPDynamicDatabase::addPowerSample(uint64_t deviceId, double timestamp,
          const std::vector<uint64_t>& values)
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->addPowerSample(timestamp, values);
  }

  std::vector<counters::Sample>
  VPDynamicDatabase::getPowerSamples(uint64_t deviceId)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->getPowerSamples();
  }

  void VPDynamicDatabase::addAIESample(uint64_t deviceId, double timestamp,
          const std::vector<uint64_t>& values)
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->addAIESample(timestamp, values);
  }

  std::vector<counters::Sample>
  VPDynamicDatabase::getAIESamples(uint64_t deviceId)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->getAIESamples();
  }

  std::vector<counters::Sample>
  VPDynamicDatabase::moveAIESamples(uint64_t deviceId)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->moveAIESamples();
  }

  void VPDynamicDatabase::addAIEDebugSample(uint64_t deviceId, uint8_t col,
          uint8_t row, uint32_t value, uint64_t offset, std::string name) 
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->addAIEDebugSample(col, row, value, offset, name);
  }

  std::vector<xdp::aie::AIEDebugDataType>
  VPDynamicDatabase::getAIEDebugSamples(uint64_t deviceId)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->getAIEDebugSamples();
  }

  std::vector<xdp::aie::AIEDebugDataType>
  VPDynamicDatabase::moveAIEDebugSamples(uint64_t deviceId)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->moveAIEDebugSamples();
  }

  void VPDynamicDatabase::addAIETimerSample(uint64_t deviceId, unsigned long timestamp1,
          unsigned long timestamp2, const std::vector<uint64_t>& values)
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->addAIETimerSample(timestamp1, timestamp2, values);
  }

  std::vector<counters::DoubleSample>
  VPDynamicDatabase::getAIETimerSamples(uint64_t deviceId)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->getAIETimerSamples();
  }

  void VPDynamicDatabase::setPLTraceBufferFull(uint64_t deviceId, bool val)
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->setPLTraceBufferFull(val);
  }

  bool VPDynamicDatabase::isPLTraceBufferFull(uint64_t deviceId)
  {
    auto device_db = getDeviceDB(deviceId);
    return device_db->isPLTraceBufferFull();
  }

  void VPDynamicDatabase::
  setPLDeadlockInfo(uint64_t deviceId, const std::string& info)
  {
    auto device_db = getDeviceDB(deviceId);
    device_db->setPLDeadlockInfo(info);
  }

  std::string VPDynamicDatabase::
  getPLDeadlockInfo()
  {
    std::lock_guard<std::mutex> lock(deviceDBLock);
    std::string info;
    for (const auto& d : devices)
      info += d.second->getPLDeadlockInfo();
    return info;
  }
}
