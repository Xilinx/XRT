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

#ifndef LOP_PLUGIN_DOT_H
#define LOP_PLUGIN_DOT_H

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  // For low overhead profiling, we can add extra computation to the
  //  setup and teardown of the plugin as long as the actual monitoring 
  //  is as minimal as possible.

  class LowOverheadProfilingPlugin : public XDPPlugin
  {
  private:
    static const char* APIs[] ;
  public:
    LowOverheadProfilingPlugin() ;
    ~LowOverheadProfilingPlugin() ;
  } ;

}

#endif
