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

#ifndef AIE_DB_DOT_H
#define AIE_DB_DOT_H

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

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

    std::mutex traceLock; // Protects "traceData" vector

  public:
    AIEDB() = default;
    ~AIEDB();

    void addAIETraceData(uint64_t strmIndex, void* buffer, uint64_t bufferSz,
                         bool copy, uint64_t numTraceStreams);
    aie::TraceDataType* getAIETraceData(uint64_t strmIndex);

    inline
    void addAIESample(double timestamp, const std::vector<uint64_t>& values)
    { samples.addSample({timestamp, values}); }

    inline
    std::vector<counters::Sample> getAIESamples()
    { return std::move(samples.getSamples());  }
  };

} // end namespace xdp

#endif
