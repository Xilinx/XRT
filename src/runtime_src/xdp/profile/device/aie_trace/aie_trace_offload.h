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

#ifndef XDP_PROFILE_AIE_TRACE_OFFLOAD_H_
#define XDP_PROFILE_AIE_TRACE_OFFLOAD_H_

#include "aie_trace_offload_base.h"
#include "xdp/profile/device/tracedefs.h"

namespace xdp {

class PLDeviceIntf;
class AIETraceLogger;

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

class AIETraceOffload : public AIETraceOffloadBase {
  public:
    AIETraceOffload(void* handle, uint64_t id,
                    PLDeviceIntf*, AIETraceLogger*,
                    bool     isPlio,
                    uint64_t totalSize,
                    uint64_t numStrm
                   );
    virtual ~AIETraceOffload();

public:
    virtual bool initReadTrace();
    virtual void endReadTrace();
    virtual bool isTraceBufferFull();
    virtual void startOffload();
    virtual void stopOffload();

/*
 * XRT_NATIVE_BUILD is set only for x86 builds
 * Only compile this on edge+versal build
 */
#if defined (XRT_ENABLE_AIE) && ! defined (XRT_X86_BUILD)
    std::vector<AIETraceGmioDMAInst> gmioDMAInsts;
#endif

private:
    void readTracePLIO(bool final);
    void readTraceGMIO(bool final);
    bool setupPSKernel();
    void continuousOffload();
    bool keepOffloading();
    void offloadFinished();
    void checkCircularBufferSupport();
    uint64_t syncAndLog(uint64_t index);
    uint64_t searchWrittenBytes(void * buf, uint64_t bytes);
};

}

#endif
