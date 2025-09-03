// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved
#ifndef AIE_TRACE_OFFLOAD_MANAGER_H
#define AIE_TRACE_OFFLOAD_MANAGER_H
#include <iostream>
#include <sstream>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include "core/common/message.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "xdp/profile/writer/aie_trace/aie_trace_writer.h"
#include "xdp/profile/plugin/aie_trace/aie_trace_impl.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/database.h"

#ifdef XDP_CLIENT_BUILD
#include "xdp/profile/device/aie_trace/client/aie_trace_offload_client.h"
#elif XDP_VE2_BUILD
#include "xdp/profile/device/aie_trace/ve2/aie_trace_offload_ve2.h"
#else
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#endif

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}


namespace xdp {

  struct AIETraceOffloadData {
    bool valid = false;
    std::unique_ptr<AIETraceLogger> logger;
    std::unique_ptr<AIETraceOffload> offloader;
  };

class AIETraceOffloadManager {
  private:
    void startPLIOOffload(bool continuousTrace, uint64_t offloadIntervalUs);
    void startGMIOOffload(bool continuousTrace, uint64_t offloadIntervalUs);
    uint64_t checkAndCapToBankSize(uint8_t memIndex, uint64_t desired);

    uint64_t deviceID;
    VPDatabase* db;
    AieTraceImpl* aieTraceImpl = nullptr;
    AIETraceOffloadData plio;
    AIETraceOffloadData gmio;
    bool offloadEnabledPLIO = false;
    bool offloadEnabledGMIO = false;

  public:
    AIETraceOffloadManager(uint64_t device_id, VPDatabase* database, AieTraceImpl* impl=nullptr);
    void initPLIO(void* handle, PLDeviceIntf* deviceIntf, uint64_t bufSize, uint64_t numStreams, XAie_DevInst* devInst);

    // TODO: Use const references for parameters where applicable
  #ifdef XDP_CLIENT_BUILD
    void initGMIO(void* handle, PLDeviceIntf* deviceIntf,
                  uint64_t bufSize, uint64_t numStreams, xrt::hw_context context,
                  std::shared_ptr<AieTraceMetadata> metadata);
  #else
    void initGMIO(void* handle, PLDeviceIntf* deviceIntf, uint64_t bufSize, uint64_t numStreams, XAie_DevInst* devInst);
  #endif

    void startOffload(bool continuousTrace, uint64_t offloadIntervalUs);
    bool initReadTraces();
    void flushAll(bool warn);
    void flushOffloader(const std::unique_ptr<AIETraceOffload>& offloader, bool warn);
    void createTraceWriters(uint64_t numStreamsPLIO, uint64_t numStreamsGMIO,
                            std::vector<VPWriter*>& writers);
    bool configureAndInitPLIO(void* handle, PLDeviceIntf* deviceIntf,
                              uint64_t desiredBufSize, uint64_t numStreamsPLIO, XAie_DevInst* devInst);
    bool configureAndInitGMIO(void* handle, PLDeviceIntf* deviceIntf,
                              uint64_t desiredBufSize, uint64_t numStreamsGMIO
  #ifdef XDP_CLIENT_BUILD
                              , const xrt::hw_context& hwctx, const std::shared_ptr<AieTraceMetadata>& md
  #else
                              , XAie_DevInst* devInst
  #endif
                              );

}; // class AIETraceOffloadManager

} //namespace xdp

#endif // AIE_TRACE_OFFLOAD_MANAGER_H