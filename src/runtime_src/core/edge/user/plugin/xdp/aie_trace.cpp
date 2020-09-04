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


namespace xdpaietrace {

  void load_xdp_aie_trace_plugin()
  {
    static xrt_core::module_loader xdp_aie_trace_loader("xdp_aie_trace_plugin",
                                                        register_aie_trace_callbacks,
                                                        warning_aie_trace_callbacks);
  }

  std::function<void (void*)> update_aie_device_cb;
  std::function<void (void*)> flush_aie_device_cb;

  void register_aie_trace_callbacks(void* handle)
  {
#ifdef XRT_CORE_BUILD_WITH_DL
    typedef void (*ftype)(void*) ;
    update_aie_device_cb = (ftype)(xrt_core::dlsym(handle, "updateAIEDevice")) ;
    if (xrt_core::dlerror() != NULL) update_aie_device_cb = nullptr ;

    flush_aie_device_cb = (ftype)(xrt_core::dlsym(handle, "flushAIEDevice")) ;
    if (xrt_core::dlerror() != NULL) flush_aie_device_cb = nullptr ;
#endif
  }

  void warning_aie_trace_callbacks()
  {
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
}
