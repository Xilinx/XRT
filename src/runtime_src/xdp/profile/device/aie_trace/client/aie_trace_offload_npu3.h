// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved

#ifndef XDP_PROFILE_AIE_TRACE_OFFLOAD_NPU3_H_
#define XDP_PROFILE_AIE_TRACE_OFFLOAD_NPU3_H_

#include "xdp/profile/device/aie_trace/aie_trace_offload_base.h"

#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/config.h"
#include "xdp/profile/device/common/npu3_transaction.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"

#include <thread>

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp {

class PLDeviceIntf;
class AIETraceLogger;

class AIETraceOffloadNPU3 : public AIETraceOffloadBase {
  public:
    AIETraceOffloadNPU3(void* handle, uint64_t id,
                        PLDeviceIntf*, AIETraceLogger*,
                        bool     isPlio,
                        uint64_t totalSize,
                        uint64_t numStrm,
                        xrt::hw_context context,
                        std::shared_ptr<AieTraceMetadata> metadata
                       );
    virtual ~AIETraceOffloadNPU3();

  public:
    virtual bool initReadTrace();
    virtual void endReadTrace();
    virtual void startOffload();
    virtual void stopOffload();

    bool isTraceBufferFull() {return false;};

  private:
    //Internal use only
    // Set this for verbose trace offload
    bool m_debug = false;
    XAie_DevInst aieDevInst = {0};
    std::unique_ptr<aie::NPU3Transaction> tranxHandler;

    xrt::hw_context context;
    std::shared_ptr<AieTraceMetadata> metadata;
    std::vector<xrt::bo> xrt_bos;

  private:
    void readTraceGMIO(bool final);
    void continuousOffload();
    bool keepOffloading();
    void offloadFinished();
    uint64_t syncAndLog(uint64_t index);
    std::function<void(bool)> mReadTrace;
    uint64_t searchWrittenBytes(void * buf, uint64_t bytes);
};

}

#endif
