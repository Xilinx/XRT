/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

#ifndef XDP_PROFILE_AIE_TRACE_OFFLOAD_VE2_H_
#define XDP_PROFILE_AIE_TRACE_OFFLOAD_VE2_H_

#include "xdp/profile/device/tracedefs.h"
#include "shim/aie/aie.h"


namespace xdp {

class PLDeviceIntf;
class AIETraceLogger;

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

struct AIETraceGmioDMAInst
{
  // C_RTS Shim DMA to where this GMIO object is mapped
  XAie_DmaDesc shimDmaInst;
  XAie_LocType gmioTileLoc;
};

enum class AIEOffloadThreadStatus {
  IDLE,
  RUNNING,
  STOPPING,
  STOPPED
};

class AIETraceOffload 
{
  public:
    AIETraceOffload(void* handle, uint64_t id,
                    PLDeviceIntf*, AIETraceLogger*,
                    bool     isPlio,
                    uint64_t totalSize,
                    uint64_t numStrm
                   );

    virtual ~AIETraceOffload();

public:
    bool initReadTrace();
    void endReadTrace();
    bool isTraceBufferFull();
    void startOffload();
    void stopOffload();

    inline AIETraceLogger* getAIETraceLogger() { return traceLogger; }
    inline void setContinuousTrace() { traceContinuous = true; }
    inline bool continuousTrace()    { return traceContinuous; }
    inline void setOffloadIntervalUs(uint64_t v) { offloadIntervalUs = v; }

    inline AIEOffloadThreadStatus getOffloadStatus() {
      std::lock_guard<std::mutex> lock(statusLock);
      return offloadStatus;
    };

    void readTrace(bool final) {mReadTrace(final);};

private:

    void*           deviceHandle;
    uint64_t        deviceId;
    PLDeviceIntf*     deviceIntf;
    AIETraceLogger* traceLogger;

    bool isPLIO;
    uint64_t totalSz;
    uint64_t numStream;
    uint64_t bufAllocSz;
    std::vector<AIETraceBufferInfo>  buffers;

    //Internal use only
    // Set this for verbose trace offload
    bool m_debug = false;
    std::vector<AIETraceGmioDMAInst> gmioDMAInsts;


    // Continuous Trace Offload (For PLIO)
    bool traceContinuous;
    uint64_t offloadIntervalUs;
    bool bufferInitialized;
    std::mutex statusLock;
    AIEOffloadThreadStatus offloadStatus;
    std::thread offloadThread;

    //Circular Buffer Tracking
    bool mEnCircularBuf;
    bool mCircularBufOverwrite;

private:
    void readTracePLIO(bool final);
    void readTraceGMIO(bool final);
    bool setupPSKernel();
    void continuousOffload();
    bool keepOffloading();
    void offloadFinished();
    void checkCircularBufferSupport();
    uint64_t syncAndLog(uint64_t index);
    std::function<void(bool)> mReadTrace;
    uint64_t searchWrittenBytes(void * buf, uint64_t bytes);
};

}

#endif
