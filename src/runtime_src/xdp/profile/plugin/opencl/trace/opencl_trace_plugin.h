/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#ifndef OPENCL_TRACE_PLUGIN_DOT_H
#define OPENCL_TRACE_PLUGIN_DOT_H

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  class OpenCLTracePlugin : public XDPPlugin
  {
  private:
    // There should only ever be one instance of the OpenCLTracePlugin,
    // and it is destroyed at the end of the program.  To prevent access after
    // the instance has been destroyed we have this guard.
    static bool live;

  protected:
    virtual void emulationSetup() ;

  public:
    OpenCLTracePlugin() ;
    ~OpenCLTracePlugin() ;

    static bool alive() { return OpenCLTracePlugin::live; }
  } ;

}

#endif
