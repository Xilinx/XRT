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

#include <functional>
#include "aie_trace.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"

namespace xdpaietrace {

  void load_xdp_aie_trace_plugin()
  {
    static xrt_core::module_loader xdp_aie_trace_loader("xdp_aie_trace_plugin",
                                                        register_aie_trace_callbacks,
                                                        aie_trace_warning_function,
                                                        aie_trace_error_function);
  }

  std::function<void (void*)> update_aie_device_cb;
  std::function<void (void*)> flush_aie_device_cb;
  std::function<void (void*)> finish_flush_aie_device_cb;

  void register_aie_trace_callbacks(void* handle)
  {
#ifdef XRT_CORE_BUILD_WITH_DL
    typedef void (*ftype)(void*) ;
    update_aie_device_cb = (ftype)(xrt_core::dlsym(handle, "updateAIEDevice")) ;
    if (xrt_core::dlerror() != NULL) update_aie_device_cb = nullptr ;

    flush_aie_device_cb = (ftype)(xrt_core::dlsym(handle, "flushAIEDevice")) ;
    if (xrt_core::dlerror() != NULL) flush_aie_device_cb = nullptr ;

    finish_flush_aie_device_cb = (ftype)(xrt_core::dlsym(handle, "finishFlushAIEDevice")) ;
    if (xrt_core::dlerror() != NULL) finish_flush_aie_device_cb = nullptr ;
#endif
  }

  void aie_trace_warning_function()
  {
  }

  int aie_trace_error_function()
  {
    if(xrt_core::config::get_profile() || xrt_core::config::get_timeline_trace()) {
      // OpenCL profiling and/or trace is enabled in xrt.ini config. So, disable AIE Trace Offload as both of these flows are not supported together.
      return 1;
    }
    return 0 ;
  }

} // end namespace xdpaietrace

namespace xdpaie {

  void update_aie_device(void* handle)
  {
    if (xdpaietrace::update_aie_device_cb != nullptr) {
      xdpaietrace::update_aie_device_cb(handle) ;
    }
  }

  void flush_aie_device(void* handle)
  {
    if (xdpaietrace::flush_aie_device_cb != nullptr) {
      xdpaietrace::flush_aie_device_cb(handle) ;
    }
  }

  void finish_flush_aie_device(void* handle)
  {
    if (xdpaietrace::finish_flush_aie_device_cb != nullptr) {
      xdpaietrace::finish_flush_aie_device_cb(handle) ;
    }
  }
}
