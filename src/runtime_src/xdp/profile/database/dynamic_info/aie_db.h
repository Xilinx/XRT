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

#ifndef AIE_DB_DOT_H
#define AIE_DB_DOT_H

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "xdp/config.h"
#include "xdp/profile/database/dynamic_info/samples.h"
#include "xdp/profile/database/dynamic_info/types.h"

namespace xdp {

  // This class abstracts the dynamic information related to AIE executions.
  // This includes trace information as well as profiling samples.
  class AIEDB
  {
  private:
    aie::TraceDataVector traceData;

    SampleContainer samples;
    DoubleSampleContainer timerSamples;
    AIEDebugContainer aieDebugSamples;

    std::mutex traceLock; // Protects "traceData" vector

    // This is the amount of AIESample threshold AIEDB stores before it flushes to the disk
    static constexpr uint64_t sampleThreshold = 100000;

  public:
    AIEDB() = default;
    XDP_CORE_EXPORT ~AIEDB();

    void addAIETraceData(uint64_t strmIndex, void* buffer, uint64_t bufferSz,
                         bool copy, uint64_t numTraceStreams);
    aie::TraceDataType* getAIETraceData(uint64_t strmIndex);

    void addAIESample(double timestamp, const std::vector<uint64_t>& values);

    inline
    void addAIETimerSample(unsigned long timestamp1, unsigned long timestamp2,
                           const std::vector<uint64_t>& values)
    { timerSamples.addSample({timestamp1, timestamp2, values}); }

    inline
    void addAIEDebugSample(uint8_t col, uint8_t row, uint32_t value, uint64_t offset, std::string name)
    { aieDebugSamples.addAIEDebugData({col, row, value, offset, name}); }

    inline
    std::vector<counters::Sample> getAIESamples()
    { return samples.getSamples();  }

    inline
    std::vector<counters::Sample> moveAIESamples()
    { return samples.moveSamples(); }

    inline
    std::vector<counters::DoubleSample> getAIETimerSamples()
    { return timerSamples.getSamples();  }

    inline
    std::vector<xdp::aie::AIEDebugDataType> getAIEDebugSamples()
    { return aieDebugSamples.getAIEDebugData();  }

    inline
    std::vector<xdp::aie::AIEDebugDataType> moveAIEDebugSamples()
    { return aieDebugSamples.moveAIEDebugData(); }
  };

} // end namespace xdp

#endif
