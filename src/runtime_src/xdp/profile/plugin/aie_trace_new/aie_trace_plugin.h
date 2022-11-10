/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef AIE_TRACE_PLUGIN_H
#define AIE_TRACE_PLUGIN_H

#include <cstdint>

#include "xdp/profile/plugin/aie_trace_new/aie_trace_impl.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/device/aie_trace/aie_trace_offload.h"
#include "xdp/profile/database/events/creator/aie_trace_data_logger.h"
#include "aie_trace_metadata.h"

namespace xdp {

  class AieTracePluginUnified : public XDPPlugin {
  public:
    AieTracePluginUnified();
    virtual ~AieTracePluginUnified();
    void updateAIEDevice(void* handle);
    void flushAIEDevice(void* handle);
    void finishFlushAIEDevice(void* handle);
    virtual void writeAll(bool openNewFiles) override;
    static bool alive();

  private:
    uint64_t getDeviceIDFromHandle(void* handle);
    void flushOffloader(const std::unique_ptr<AIETraceOffload>& offloader, bool warn);

  private:
    static bool live;
    struct AIEData {
      uint64_t deviceID;
      bool valid;
      DeviceIntf* devIntf;
      std::unique_ptr<AIETraceOffload> offloader;
      std::unique_ptr<AIETraceLogger> logger;
      std::unique_ptr<AieTraceImpl> implementation;
      std::shared_ptr<AieTraceMetadata> metadata;
    };
    std::map<void*, AIEData>  handleToAIEData;
  };

}   

#endif
