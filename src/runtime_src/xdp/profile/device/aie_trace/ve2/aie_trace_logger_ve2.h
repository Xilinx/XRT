/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef XDP_PROFILE_AIE_TRACE_VE2_LOGGER_H
#define XDP_PROFILE_AIE_TRACE_VE2_LOGGER_H

#include <cstdint>
#include<iostream>

namespace xdp {

// Interface class
class AIETraceLogger
{
public:
  AIETraceLogger() {}
  virtual ~AIETraceLogger() {}

  virtual void addAIETraceData(uint64_t strmIndex, void* buffer, uint64_t bufferSz, bool copy) = 0;
};

}
#endif
