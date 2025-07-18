/**
 * Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_PROFILE_AIE_TRACE_OFFLOAD_BASE_H_
#define XDP_PROFILE_AIE_TRACE_OFFLOAD_BASE_H_

#include "xdp/profile/device/tracedefs.h"

#include <cstddef>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>

/*
 * XRT_NATIVE_BUILD is set only for x86 builds
 * We can only include/compile aie specific headers, when compiling for edge+versal.
 *
 * AIE specific edge code that needs to be protected includes:
 * 1. Header file inclusions
 * 2. GMIO driver specific definitions
 * 3. GMIO driver calls to configure shim DMA
 *
 * When XRT_NATIVE_BUILD is defined, the offloading structure is:
 * 1. For PL offload, same as edge
 * 2. For GMIO offload, ps kernel needs to be used to initialize and read data
 */

#if defined (XRT_ENABLE_AIE) && ! defined (XRT_X86_BUILD)
#include "core/edge/user/aie/aie.h"
#endif

namespace xdp {

class PLDeviceIntf;
class AIETraceLogger;

#define MM2S_CONTROL 2
#define S2MM_TRACE   3

#define debug_stream if(!m_debug); else std::cout

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

/*
 * XRT_NATIVE_BUILD is set only for x86 builds
 * Only compile this on edge+versal build
 */
#if defined (XRT_ENABLE_AIE) && ! defined (XRT_X86_BUILD)
struct AIETraceGmioDMAInst
{
  // C_RTS Shim DMA to where this GMIO object is mapped
  XAie_DmaDesc shimDmaInst;
  XAie_LocType gmioTileLoc;
};
#endif

enum class AIEOffloadThreadStatus {
  IDLE,
  RUNNING,
  STOPPING,
  STOPPED
};


class AIETraceOffloadBase
{
  public:
    AIETraceOffloadBase(void* handle, uint64_t id, PLDeviceIntf* dInt, 
                        AIETraceLogger* logger, bool isPlio,
                        uint64_t totalSize, uint64_t numStrm)
      : deviceHandle(handle), deviceId(id), deviceIntf(dInt), 
        traceLogger(logger), isPLIO(isPlio), totalSz(totalSize), 
        numStream(numStrm), traceContinuous(false), offloadIntervalUs(0), 
        bufferInitialized(false), offloadStatus(AIEOffloadThreadStatus::IDLE), 
        mEnCircularBuf(false), mCircularBufOverwrite(false) {}
    AIETraceOffloadBase() = delete;
    virtual ~AIETraceOffloadBase() {}

public:
    virtual bool initReadTrace() = 0;
    virtual void endReadTrace() = 0;
    virtual bool isTraceBufferFull() = 0;
    virtual void startOffload() = 0;
    virtual void stopOffload() = 0;

    inline AIETraceLogger* getAIETraceLogger() { return traceLogger; }
    inline void setContinuousTrace() { traceContinuous = true; }
    inline bool continuousTrace()    { return traceContinuous; }
    inline void setOffloadIntervalUs(uint64_t v) { offloadIntervalUs = v; }

    inline AIEOffloadThreadStatus getOffloadStatus() {
      std::lock_guard<std::mutex> lock(statusLock);
      return offloadStatus;
    };

    void readTrace(bool final) {mReadTrace(final);};

protected:
    void*           deviceHandle;
    uint64_t        deviceId;
    PLDeviceIntf*   deviceIntf;
    AIETraceLogger* traceLogger;

    bool isPLIO;
    uint64_t totalSz;
    uint64_t numStream;
    uint64_t bufAllocSz;
    std::vector<AIETraceBufferInfo>  buffers;

    // Internal use only
    // Set this for verbose trace offload
    bool m_debug = false;

    // Continuous Trace Offload (For PLIO)
    bool traceContinuous;
    uint64_t offloadIntervalUs;
    bool bufferInitialized;
    std::mutex statusLock;
    AIEOffloadThreadStatus offloadStatus;
    std::thread offloadThread;

    // Circular Buffer Tracking
    bool mEnCircularBuf;
    bool mCircularBufOverwrite;

    std::function<void(bool)> mReadTrace;
};

}

#endif
