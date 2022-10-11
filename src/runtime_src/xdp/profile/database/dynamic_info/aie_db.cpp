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

#include <cstring>

#include "xdp/profile/database/dynamic_info/aie_db.h"

namespace xdp {

  AIEDB::~AIEDB()
  {
    std::lock_guard<std::mutex> lock(traceLock);

    for (auto info : traceData)
      delete info;
    traceData.clear();
  }

  void AIEDB::addAIETraceData(uint64_t strmIndex, void* buffer,
                              uint64_t bufferSz, bool copy,
                              uint64_t numTraceStreams)
  {
    std::lock_guard<std::mutex> lock(traceLock);

    if (traceData.size() == 0)
      traceData.resize(numTraceStreams);

    if (traceData[strmIndex] == nullptr)
      traceData[strmIndex] = new aie::TraceDataType;

    unsigned char* trace_buffer = static_cast<unsigned char*>(buffer);
    if (copy) {
      // We need to copy data as it may be overwritten by datamover
      trace_buffer = new unsigned char[bufferSz];
      std::memcpy(trace_buffer, buffer, bufferSz);
    }
    traceData[strmIndex]->buffer.push_back(trace_buffer);
    traceData[strmIndex]->bufferSz.push_back(bufferSz);
    traceData[strmIndex]->owner = copy;
  }

  aie::TraceDataType* AIEDB::getAIETraceData(uint64_t strmIndex)
  {
    std::lock_guard<std::mutex> lock(traceLock);

    if (traceData.size() == 0)
      return nullptr;

    auto data = traceData[strmIndex];
    traceData[strmIndex] = new aie::TraceDataType;
    return data;
  }

} // end namespace xdp
