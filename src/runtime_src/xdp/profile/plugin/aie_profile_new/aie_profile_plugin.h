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

#ifndef XDP_AIE_PLUGIN_DOT_H
#define XDP_AIE_PLUGIN_DOT_H

#include "xdp/profile/plugin/aie_profile_new/aie_profile_impl.h"
#include "xdp/profile/plugin/aie_profile_new/aie_profile_metadata.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"


namespace xdp {

  class AieProfilePlugin : public XDPPlugin
  {
  public:
    XDP_EXPORT AieProfilePlugin();
    XDP_EXPORT ~AieProfilePlugin();
    XDP_EXPORT void updateAIEDevice(void* handle);
    XDP_EXPORT void endPollforDevice(void* handle);
    XDP_EXPORT static bool alive();

  private:
    uint64_t getDeviceIDFromHandle(void* handle);
    void pollAIECounters(uint32_t index, void* handle);
    void endPoll();


  private:
    uint32_t mIndex = 0;

    static bool live;
    struct AIEData {
      uint64_t deviceID;
      std::unique_ptr<AieProfileImpl> implementation;
      std::shared_ptr<AieProfileMetadata> metadata;
      std::atomic<bool> threadCtrlBool;
      std::thread thread;

    };
    std::map<void*, AIEData>  handleToAIEData;

  };

} // end namespace xdp

#endif
