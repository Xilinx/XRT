/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef OPENCL_COUNTERS_PLUGIN_DOT_H
#define OPENCL_COUNTERS_PLUGIN_DOT_H

#include "xocl/core/device.h"

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  class OpenCLCountersProfilingPlugin : public XDPPlugin
  {
  private:
    std::shared_ptr<xocl::platform> platform ;

  protected:
    virtual void emulationSetup() ;

  public:
    OpenCLCountersProfilingPlugin() ;
    ~OpenCLCountersProfilingPlugin() ;

    // For emulation based flows we need to convert real time into
    //  estimated device time to match what we reported previously
    uint64_t convertToEstimatedTimestamp(uint64_t realTimeStamp) ;
  } ;

} // end namespace xdp

#endif
