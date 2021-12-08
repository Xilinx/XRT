/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef PL_DEADLOCK_PLUGIN_DOT_H
#define PL_DEADLOCK_PLUGIN_DOT_H

#include <vector>
#include <atomic>

#include "xdp/profile/plugin/device_offload/device_offload_plugin.h"

namespace xdp {

  class PLDeadlockPlugin : public XDPPlugin
  {
  private:
    XDP_EXPORT virtual void pollDeadlock(uint32_t index);
  
  private:
    uint32_t mPollingIntervalMs = 100;
    std::atomic <bool> mAtomicKeepPolling;
    std::vector<std::thread> mThreadVector;

  public:
    XDP_EXPORT PLDeadlockPlugin();
    XDP_EXPORT ~PLDeadlockPlugin();

    XDP_EXPORT virtual void updateDevice(void* handle);

    // Virtual functions from XDPPlugin
    XDP_EXPORT virtual void writeAll(bool openNewFiles) ;
  };

} // end namespace xdp

#endif
