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

#ifndef VP_DYNAMIC_EVENT_DATABASE_DOT_H
#define VP_DYNAMIC_EVENT_DATABASE_DOT_H

#include <atomic>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "core/common/uuid.h"
#include "core/include/xdp/counters.h"

#include "xdp/config.h"
#include "xdp/profile/database/dynamic_info/device_db.h"
#include "xdp/profile/database/dynamic_info/host_db.h"
#include "xdp/profile/database/dynamic_info/string_table.h"
#include "xdp/profile/database/dynamic_info/types.h"
#include "xdp/profile/database/events/vtf_event.h"

namespace xdp {

  // Forward declarations
  class VPDatabase ;

  // The dynamic database is responsible for storing all of the events
  // and samples that are generated during program execution.  Data will
  // be added by different plugins and can be retrieved by any writer.
  // The events stored in the database are owned by the database and
  // the database is responsible for cleaning up the memory unless
  // the events are moved.
  class VPDynamicDatabase
  {
  private:
    // Parent pointer to containing database
    VPDatabase* db;

    // Host related information.  There will only be one copy of host
    // information per execution.  This abstracts the host API event
    // storage and matching of start with end events.  User level events
    // are also stored here as well as dependency information.
    std::unique_ptr<HostDB> host = nullptr;

    // Device related information.  There can be multiple devices active
    // in each execution.  This abstracts the device events and the
    // matching of start with end device events.
    std::map<uint64_t, std::unique_ptr<DeviceDB>> devices;
    DeviceDB* getDeviceDB(uint64_t deviceId);

    // A unique event id for every event added to the database, both host
    // and device events.  It starts with 1 so we can use 0 as an
    // indicator of NULL.
    std::atomic<uint64_t> eventId;
    void issueId(VTFEvent* event);

    // For all strings associated with events, we keep only one unique
    // copy and will use uint64_t numbers as references instead.
    StringTable stringTable;

    std::mutex deviceDBLock; // Protects the "devices" map

    void addHostEvent(VTFEvent* event);
    void addDeviceEvent(uint64_t deviceId, VTFEvent* event);

  public:
    XDP_CORE_EXPORT VPDynamicDatabase(VPDatabase* d);
    XDP_CORE_EXPORT ~VPDynamicDatabase() = default;

    // For multiple xclbin designs, add a device event that marks the
    // transition from one xclbin to another
    XDP_CORE_EXPORT void markXclbinEnd(uint64_t deviceId);

    // Add an event in sorted order in the database
    XDP_CORE_EXPORT void addEvent(VTFEvent* event);

    // Add an event to the database to be sorted later when we write
    XDP_CORE_EXPORT void addUnsortedEvent(VTFEvent* event);

    // For API events, find the event id of the start event for an end event
    XDP_CORE_EXPORT void markStart(uint64_t functionID, uint64_t eventID) ;
    XDP_CORE_EXPORT uint64_t matchingStart(uint64_t functionID) ;

    // For user level events, find the label and tooltip associated
    XDP_CORE_EXPORT void markRange(uint64_t functionID,
			      std::pair<const char*, const char*> desc,
			      uint64_t startTimestamp) ;
    XDP_CORE_EXPORT UserRangeInfo matchingRange(uint64_t functionID) ;

    // For Device Events, find matching start for end event
    XDP_CORE_EXPORT
    void markDeviceEventStart(uint64_t deviceId,
                              uint64_t monitorId,
                              DeviceEventInfo& info) ;
    XDP_CORE_EXPORT
    DeviceEventInfo
    matchingDeviceEventStart(uint64_t deviceId,
                             uint64_t monitorId,
                             VTFEventType type) ;
    XDP_CORE_EXPORT bool hasMatchingDeviceEventStart(uint64_t deviceId,
                                                uint64_t monitiorId,
                                                VTFEventType type) ;

    // For API events that we cannot guarantee have unique IDs across all
    //  the plugins, we have a seperate matching of start to end
    XDP_CORE_EXPORT void markXRTUIDStart(uint64_t uid, uint64_t eventID) ;
    XDP_CORE_EXPORT uint64_t matchingXRTUIDStart(uint64_t uid);

    XDP_CORE_EXPORT void markEventPairStart(uint64_t functionId, const EventPair& events);
    XDP_CORE_EXPORT EventPair matchingEventPairStart(uint64_t functionId);

    // A lookup into the string table.  If the string isn't already in
    // the string table it will be added
    inline uint64_t addString(const std::string& value)
    { return stringTable.addString(value); }

    // A function that iterates on the dynamic events and returns
    // copies of the events based upon the filter passed in
    XDP_CORE_EXPORT
    std::vector<VTFEvent*>
    copySortedHostEvents(std::function<bool(VTFEvent*)> filter);

    // Erase events from db and transfer ownership to caller
    XDP_CORE_EXPORT std::vector<std::unique_ptr<VTFEvent>> moveSortedHostEvents(std::function<bool(VTFEvent*)> filter);
    XDP_CORE_EXPORT std::vector<VTFEvent*> moveUnsortedHostEvents(std::function<bool(VTFEvent*)> filter);
    XDP_CORE_EXPORT std::vector<std::unique_ptr<VTFEvent>> moveDeviceEvents(uint64_t deviceId);

    XDP_CORE_EXPORT bool deviceEventsExist(uint64_t deviceId);
    XDP_CORE_EXPORT bool hostEventsExist(std::function<bool(VTFEvent*)> filter);

    XDP_CORE_EXPORT void setCounterResults(uint64_t deviceId,
				      xrt_core::uuid uuid,
				      xdp::CounterResults& values) ;
    XDP_CORE_EXPORT xdp::CounterResults getCounterResults(uint64_t deviceId,
                                                     xrt_core::uuid uuid) ;

    // A function that each writer calls to dump the string table
    inline void dumpStringTable(std::ofstream& fout)
    { stringTable.dumpTable(fout); }

    // OpenCL mappings and dependencies
    XDP_CORE_EXPORT void addOpenCLMapping(uint64_t openclID, uint64_t eventID, uint64_t startID) ;
    XDP_CORE_EXPORT std::pair<uint64_t, uint64_t>
    lookupOpenCLMapping(uint64_t openclID) ;

    XDP_CORE_EXPORT void addDependency(uint64_t id, uint64_t dependency) ;
    XDP_CORE_EXPORT std::map<uint64_t, std::vector<uint64_t>> getDependencyMap() ;

    // Add and get AIE Trace Data Buffer
    XDP_CORE_EXPORT void addAIETraceData(uint64_t deviceId, uint64_t strmIndex, void* buffer, uint64_t bufferSz, bool copy);
    XDP_CORE_EXPORT aie::TraceDataType* getAIETraceData(uint64_t deviceId, uint64_t strmIndex);

    // Functions that are used by counter-based plugins
    XDP_CORE_EXPORT void addPowerSample(uint64_t deviceId, double timestamp,
				   const std::vector<uint64_t>& values) ;
    XDP_CORE_EXPORT std::vector<counters::Sample> getPowerSamples(uint64_t deviceId) ;

    XDP_CORE_EXPORT void addAIESample(uint64_t deviceId, double timestamp,
				   const std::vector<uint64_t>& values);
    XDP_CORE_EXPORT void addAIEDebugSample(uint64_t deviceId, uint8_t col,
           uint8_t row,  uint32_t value, uint64_t offset, std::string name);
    XDP_CORE_EXPORT std::vector<xdp::aie::AIEDebugDataType> moveAIEDebugSamples(uint64_t deviceId);
    XDP_CORE_EXPORT std::vector<xdp::aie::AIEDebugDataType> getAIEDebugSamples(uint64_t deviceId);
    XDP_CORE_EXPORT std::vector<counters::Sample> getAIESamples(uint64_t deviceId) ;
    XDP_CORE_EXPORT std::vector<counters::Sample> moveAIESamples(uint64_t deviceId);
    XDP_CORE_EXPORT void addAIETimerSample(uint64_t deviceId, unsigned long timestamp1,
				   unsigned long timestamp2, const std::vector<uint64_t>& values) ;
    XDP_CORE_EXPORT std::vector<counters::DoubleSample> getAIETimerSamples(uint64_t deviceId) ;

    // Device Trace Buffer Fullness Status - PL
    XDP_CORE_EXPORT void setPLTraceBufferFull(uint64_t deviceId, bool val);
    XDP_CORE_EXPORT bool isPLTraceBufferFull(uint64_t deviceId);

    // Deadlock Diagnosis metadata
    XDP_CORE_EXPORT void setPLDeadlockInfo(uint64_t deviceId, const std::string& str);
    XDP_CORE_EXPORT std::string getPLDeadlockInfo();
  } ;

}

#endif
