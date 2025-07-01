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

#ifndef XDP_PROFILE_AIE_TRACE_OFFLOAD_WIN_H_
#define XDP_PROFILE_AIE_TRACE_OFFLOAD_WIN_H_

#include "xdp/profile/device/aie_trace/aie_trace_offload_base.h"

#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/config.h"
#include "xdp/profile/device/common/client_transaction.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_metadata.h"

extern "C" {
  #include <xaiengine.h>
  #include <xaiengine/xaiegbl_params.h>
}

namespace xdp {

class PLDeviceIntf;
class AIETraceLogger;

class AIETraceOffloadClient : public AIETraceOffloadBase {
  public:
    AIETraceOffloadClient(void* handle, uint64_t id,
                          PLDeviceIntf*, AIETraceLogger*,
                          bool     isPlio,
                          uint64_t totalSize,
                          uint64_t numStrm,
                          xrt::hw_context context,
                          std::shared_ptr<AieTraceMetadata> metadata
                         );
    virtual ~AIETraceOffloadClient();

  public:
    virtual bool initReadTrace();
    virtual void endReadTrace();
    virtual void startOffload();
    virtual void stopOffload();

    virtual bool isTraceBufferFull() {return false;};

  private:
    XAie_DevInst aieDevInst = {0};
    std::unique_ptr<aie::ClientTransaction> transactionHandler;

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
