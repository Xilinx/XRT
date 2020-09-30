/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/config.h"

#include "core/common/device.h"

namespace xdp {

  class AIEProfilingPlugin : public XDPPlugin
  {
  public:
    AIEProfilingPlugin();
    ~AIEProfilingPlugin();

    XDP_EXPORT
    void updateAIEDevice(void* handle);
    XDP_EXPORT
    void endPollforDevice(void* handle);

  private:
    void pollAIECounters(uint32_t index, void* handle);
    void endPoll();

  private:
    // AIE profiling uses its own thread
    unsigned int mPollingInterval;

    std::map<void*,std::atomic<bool>> thread_ctrl_map;
    std::map<void*,std::thread> thread_map;

    uint32_t mIndex = 0;
  };

} // end namespace xdp

#endif
