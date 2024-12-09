/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#define XDP_PLUGIN_SOURCE

#include "core/include/xrt/detail/xclbin.h"
#include "xdp/debug/debug_plugin.h"
#include "xdp/debug/kernel_debug_manager.h"
#include "xocl/api/plugin/xdp/debug.h"

namespace xdp {

  // This object is created when the plugin library is loaded
  static KernelDebugManager kdm ;

  static void debug_reset(const axlf* xclbin)
  {
    kdm.reset(xclbin) ;
  }

} // end namespace xdp

// The linked callback called from XRT.  This should be called
//  everytime a new xclbin is loaded
void cb_debug_reset(const axlf* xclbin) 
{
  xdp::debug_reset(xclbin) ;
}

// This function is called from XRT once when the library is initially loaded
extern "C"
void initKernelDebug()
{
  xocl::debug::register_cb_reset(cb_debug_reset) ;
}
