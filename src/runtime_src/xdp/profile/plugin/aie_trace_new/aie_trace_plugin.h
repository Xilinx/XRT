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

namespace xdp {

  class AieTracePluginUnified : public XDPPlugin {
  public:
    XDP_EXPORT AieTracePluginUnified();
    XDP_EXPORT ~AieTracePluginUnified();
    XDP_EXPORT void updateAIEDevice(void* handle);
    XDP_EXPORT void flushAIEDevice(void* handle);
    XDP_EXPORT void finishFlushAIEDevice(void* handle);
    XDP_EXPORT virtual void writeAll(bool openNewFiles);
    XDP_EXPORT static bool alive();

  private:
    uint64_t getDeviceIDFromHandle(void* handle);
    void flushOffloader(const std::unique_ptr<AIETraceOffload>& offloader, bool warn);

  private:
    static bool live;
    struct AIEData {
      uint64_t deviceID;
      bool supported;
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
