/**
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

#ifndef DEVICE_DB_DOT_H
#define DEVICE_DB_DOT_H

#include <list>
#include <map>
#include <memory>
#include <mutex>

#include "core/common/uuid.h"
#include "core/include/xdp/counters.h"

#include "xdp/profile/database/dynamic_info/aie_db.h"
#include "xdp/profile/database/dynamic_info/pl_db.h"
#include "xdp/profile/database/dynamic_info/types.h"

namespace xdp {

  // Forward declarations
  class VTFEvent;

  // There may be multiple devices visible to XRT.  This class abstracts
  // all of the information collected on a single device.  The main database
  // will have multiple instances of this class.  This covers both PL side
  // information as well as AIE information.
  class DeviceDB
  {
  private:
    // Abstract all of the dynamic information related to the PL portion
    // of the device, including monitor counters, trace, and power samples.
    PLDB pl_db;

    // Abstract all of the dynamic information related to the AIE portion
    // of the device, including profile counter samples and AIE event trace.
    AIEDB aie_db;

  public:
    DeviceDB() = default;
    ~DeviceDB() = default;

    // ****************************************************************
    // Functions to access the PL portion of the device.  These are all
    // inlined accesses to the PL database object.
    // ****************************************************************
    inline void addPLTraceEvent(VTFEvent* event) { pl_db.addEvent(event); }
    inline bool eventsExist() { return pl_db.eventsExist(); }

    inline std::vector<std::unique_ptr<VTFEvent>> moveEvents()
    { return pl_db.moveEvents(); }

    inline void markStart(uint64_t monitorId, const DeviceEventInfo& info)
    {  pl_db.markStart(monitorId, info);  }

    inline
    DeviceEventInfo findMatchingStart(uint64_t monitorId, VTFEventType type)
    { return pl_db.findMatchingStart(monitorId, type); }

    inline bool hasMatchingStart(uint64_t monitorId, VTFEventType type)
    { return pl_db.hasMatchingStart(monitorId, type); }

    inline void setPLTraceBufferFull(bool val)
    { pl_db.setPLTraceBufferFull(val); }

    inline bool isPLTraceBufferFull() { return pl_db.isPLTraceBufferFull(); }

    inline void setPLCounterResults(xrt_core::uuid uuid, CounterResults& values)
    { pl_db.setPLCounterResults(uuid, values); }
    inline CounterResults getPLCounterResults(xrt_core::uuid uuid)
    { return pl_db.getPLCounterResults(uuid); }

    inline
    void addPowerSample(double timestamp, const std::vector<uint64_t>& values)
    { pl_db.addPowerSample(timestamp, values); }

    inline std::vector<counters::Sample> getPowerSamples()
    { return pl_db.getPowerSamples(); }

    // ****************************************************************
    // Functions to access the AIE portion of the device.  These are all
    // inlined accesses to the AIE database object.
    // ****************************************************************

    inline void addAIETraceData(uint64_t strmIndex, void* buffer,
                                uint64_t bufferSz, bool copy,
                                uint64_t numStreams)
    { aie_db.addAIETraceData(strmIndex, buffer, bufferSz, copy, numStreams); }

    inline aie::TraceDataType* getAIETraceData(uint64_t strmIndex)
    { return aie_db.getAIETraceData(strmIndex); }

    inline
    void addAIESample(double timestamp, const std::vector<uint64_t>& values)
    { aie_db.addAIESample(timestamp, values);  }

    void addAIETimerSample(unsigned long timestamp1, unsigned long timestamp2,
                           const std::vector<uint64_t>& values)
    { aie_db.addAIETimerSample(timestamp1, timestamp2, values);  }

    inline
    void addAIEDebugSample(uint8_t col, uint8_t row, uint32_t value, uint64_t offset, std::string name)
    { aie_db.addAIEDebugSample(col, row, value, offset, name);  }

    inline std::vector<counters::Sample> getAIESamples()
    { return aie_db.getAIESamples();  }

    inline std::vector<counters::Sample> moveAIESamples()
    { return aie_db.moveAIESamples(); }

    inline std::vector<counters::DoubleSample> getAIETimerSamples()
    { return aie_db.getAIETimerSamples();  }

    inline void setPLDeadlockInfo(const std::string& info)
    { pl_db.setDeadlockInfo(info); }

    inline std::string& getPLDeadlockInfo()
    { return pl_db.getDeadlockInfo(); }

    inline
    std::vector<xdp::aie::AIEDebugDataType> getAIEDebugSamples()
    { return aie_db.getAIEDebugSamples();  }

    inline std::vector<xdp::aie::AIEDebugDataType> moveAIEDebugSamples()
    { return aie_db.moveAIEDebugSamples(); }


  };

} // end namespace xdp

#endif
