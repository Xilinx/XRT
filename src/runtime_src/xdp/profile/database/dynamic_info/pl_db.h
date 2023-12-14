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

#ifndef PL_DB_DOT_H
#define PL_DB_DOT_H

#include <list>
#include <map>
#include <memory>
#include <mutex>

#include "core/common/uuid.h"
#include "core/include/xdp/counters.h"

#include "xdp/profile/database/dynamic_info/samples.h"
#include "xdp/profile/database/dynamic_info/types.h"

namespace xdp {

  // Forward declarations
  class VTFEvent;

  // This abstracts away the dynamic information that we collect from
  // the PL portion of a design.
  class PLDB
  {
  private:
    // This is the amount of events we should store in the database
    // before forcing a flush.
    static constexpr uint64_t eventThreshold = 10000000;

    // Trace events.  Since the actual hardware might shuffle the order
    // of events we have to make sure that this set of events is ordered
    // based on the timestamp
    std::multimap<double, VTFEvent*> events;

    // Each monitor in the device will have a set of device event starts.
    // This map goes from monitor ID to the list of all the currently
    // outstanding device events we've observed without ends.  We keep this
    // because the hardware might drop packets due to congestion and we
    // need to detect when this happens and reconstruct the trace as best
    // as possible.
    std::map<uint64_t, std::list<DeviceEventInfo>> startEvents;

    // For the PL portion, we will have a set of the final device
    // counters in our monitors per xclbin that was loaded.
    std::map<xrt_core::uuid, CounterResults> plCounters;

    bool plTraceBufferFull = false; // Is the PL trace buffer full?

    SampleContainer powerSamples;

    std::mutex eventLock;   // For protecting the events multimap
    std::mutex startLock;   // For protecting the startEvents map
    std::mutex counterLock; // For protecting the plCounters map
    std::mutex fullLock;    // For protecting the trace buffer full bool

    // Deadlock Diagnosis String
    std::string deadlockInfo;

  public:
    PLDB()  = default;
    ~PLDB() = default;

    void addEvent(VTFEvent* event);
    bool eventsExist();

    std::vector<std::unique_ptr<VTFEvent>> moveEvents();

    void markStart(uint64_t monitorId, const DeviceEventInfo& info);
    DeviceEventInfo findMatchingStart(uint64_t monitorId, VTFEventType type);
    bool hasMatchingStart(uint64_t monitorId, VTFEventType type);

    void setPLTraceBufferFull(bool val);
    bool isPLTraceBufferFull();

    void setPLCounterResults(xrt_core::uuid uuid, CounterResults& values);
    CounterResults getPLCounterResults(xrt_core::uuid uuid);

    inline void addPowerSample(double timestamp, const std::vector<uint64_t>& values)
    { powerSamples.addSample({timestamp, values});  }
    inline std::vector<counters::Sample> getPowerSamples()
    { return powerSamples.getSamples(); }

    inline void setDeadlockInfo(const std::string& info)
    { deadlockInfo = info; }
    inline std::string& getDeadlockInfo()
    { return deadlockInfo; }
  };

} // end namespace xdp

#endif
