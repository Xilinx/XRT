/**
 * Copyright (C) 2020 Xilinx, Inc
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef POWER_PROFILING_DOT_H
#define POWER_PROFILING_DOT_H

#include <vector>
#include <string>
#include <thread>

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  class PowerProfilingPlugin : public XDPPlugin
  {
  private:
    // These are the files that will be opened and read
    static const char* powerFiles[] ;

  private:
    std::vector<std::unique_ptr<xrt::device>> xrtDevices;

    // Power profiling requires its own thread
    bool keepPolling ;
    std::thread pollingThread ;
    unsigned int pollingInterval ;
    void pollPower() ;
  public:
    PowerProfilingPlugin() ;
    ~PowerProfilingPlugin() ;

    void addDevice(void* handle) ;
  } ;

} // end namespace xdp

#endif
