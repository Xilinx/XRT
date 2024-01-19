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

#ifndef XDP_PROFILE_AIE_TRACE_OFFLOAD_WIN_H_
#define XDP_PROFILE_AIE_TRACE_OFFLOAD_WIN_H_

#include "xdp/config.h"
#include "xdp/profile/device/tracedefs.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"
#include "xdp/profile/device/common/client_transaction.h"


extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp {

class DeviceIntf;
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
                    DeviceIntf*, AIETraceLogger*,
                    bool     isPlio,
                    uint64_t totalSize,
                    uint64_t numStrm,
                    xrt::hw_context context,
                    std::shared_ptr<AieTraceMetadata> metadata
                   );
    virtual ~AIETraceOffload();

  public:
    bool initReadTrace();
    void endReadTrace();
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
    bool isTraceBufferFull() {return false;};

  private:
    void*           deviceHandle;
    uint64_t        deviceId;
    DeviceIntf*     deviceIntf;
    AIETraceLogger* traceLogger;

    bool isPLIO;
    uint64_t totalSz;
    uint64_t numStream;
    uint64_t bufAllocSz;
    std::vector<AIETraceBufferInfo> buffers;

    //Internal use only
    // Set this for verbose trace offload
    bool m_debug = false;
    XAie_DevInst aieDevInst = {0};
    std::unique_ptr<aie::ClientTransaction> transactionHandler;

    std::shared_ptr<AieTraceMetadata> metadata;
    std::vector<xrt::bo> xrt_bos;
    xrt::hw_context context;

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
    void readTraceGMIO(bool final);
    bool keepOffloading();
    void offloadFinished();
    uint64_t syncAndLog(uint64_t index);
    std::function<void(bool)> mReadTrace;
    uint64_t searchWrittenBytes(void * buf, uint64_t bytes);
};

}

#endif
