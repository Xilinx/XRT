/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "core/include/xrt/detail/xclbin.h"
#include "core/common/dlfcn.h"
#include "plugin/xdp/debug.h"
#include "core/common/module_loader.h"
#include <stdexcept>

namespace xocl {
namespace debug {

  void load_xdp_kernel_debug()
  {
    static xrt_core::module_loader 
      xdp_kernel_debug_loader("xdp_debug_plugin",
			      register_kdbg_functions,
			      nullptr) ;
  }

  void register_kdbg_functions(void* handle)
  {
    typedef void (*xdpInitType)() ;
    
    auto initFunc = (xdpInitType)(xrt_core::dlsym(handle, "initKernelDebug")) ;
    if (!initFunc)
    {
      std::string errMsg = "Failed to initialize XDP Kernel Debug library, 'initKernelDebug' symbol not found.\n" ;
      const char* dlMsg = xrt_core::dlerror() ;
      if (dlMsg != nullptr) errMsg += dlMsg ;
      throw std::runtime_error(errMsg.c_str()) ;
    }

    initFunc() ;
  }

cb_reset_type cb_reset;

void
register_cb_reset (cb_reset_type&& cb)
{
  cb_reset = std::move(cb);
}

void
reset(const axlf* xclbin)
{
  if (cb_reset)
    cb_reset(xclbin);
}

}} // debug,xocl



