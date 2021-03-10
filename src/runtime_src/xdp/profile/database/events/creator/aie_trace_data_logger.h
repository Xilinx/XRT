/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef XDP_PROFILE_AIE_TRACE_DATA_LOGGER_H
#define XDP_PROFILE_AIE_TRACE_DATA_LOGGER_H

#include "xdp/config.h"
#include "xdp/profile/device/aie_trace/aie_trace_logger.h"
#include "xdp/profile/database/database.h"

namespace xdp {

class AIETraceDataLogger : public AIETraceLogger
{
  uint64_t deviceId = 0;
  VPDatabase* db = nullptr;

public:

  XDP_EXPORT
  AIETraceDataLogger(uint64_t devId);
  XDP_EXPORT
  virtual ~AIETraceDataLogger();

  XDP_EXPORT
  virtual void addAIETraceData(uint64_t strmIndex, void* buffer, uint64_t bufferSz); 
};

}
#endif

