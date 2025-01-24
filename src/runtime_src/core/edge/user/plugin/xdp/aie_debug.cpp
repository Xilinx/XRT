/**
 * Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
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

#include "aie_debug.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include <iostream>

namespace xdp {
namespace aie {
namespace debug {
  void load()
  {
//#ifdef XRT_ENABLE_AIE
    static xrt_core::module_loader xdp_aie_loader("xdp_aie_debug_plugin",
						    register_callbacks,
						    warning_callbacks);
//#endif
  }
  std::function<void (void*)> update_device_cb;
  std::function<void (void*)> end_poll_cb;

  void register_callbacks(void* handle)
  {
    using ftype = void (*)(void*); // Device handle

    update_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "updateAIEDebugDevice"));
    if (xrt_core::dlerror() != nullptr)
      update_device_cb = nullptr;

    end_poll_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "endAIEDebugRead"));
    if (xrt_core::dlerror() != nullptr)
      end_poll_cb = nullptr;
  }

  void warning_callbacks()
  {
    // No warnings for AIE debug plugin
  }

} // end namespace debug

namespace dbg {
  void update_device(void* handle)
  {
    if (debug::update_device_cb != nullptr) {
      debug::update_device_cb(handle) ;
    }
  }

  void end_poll(void* handle)
  {
    if (debug::end_poll_cb != nullptr) {
      debug::end_poll_cb(handle) ;
    }
  }
} // end namespace dbg
} // end namespace aie
} // end namespace xdp
