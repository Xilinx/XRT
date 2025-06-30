/**
 * Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_PROFILE_AIE_TRACE_OFFLOAD_UTIL_H_
#define XDP_PROFILE_AIE_TRACE_OFFLOAD_UTIL_H_

namespace xdp {

#define S2MM_TRACE   2
#define MM2S_CONTROL 3

#define debug_stream \
if(!m_debug); else std::cout

struct AIETraceBufferInfo
{
  size_t   bufId;
//  uint64_t allocSz;	// currently all the buffers are equal size
  uint64_t usedSz;
  uint64_t offset;
  uint32_t rollover_count;
  bool     isFull;
  bool     offloadDone;

  AIETraceBufferInfo()
    : bufId(0),
      usedSz(0),
      offset(0),
      rollover_count(0),
      isFull(false),
      offloadDone(false)
  {}
};

enum class AIEOffloadThreadStatus {
  IDLE,
  RUNNING,
  STOPPING,
  STOPPED
};

}

#endif
