/**
 * Copyright (C) 2020 Xilinx, Inc
 * Copyright (C) 2025 Advanced Micro Devices, Inc. - All rights reserved
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

#include "aie_profile.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include <iostream>

namespace xdp::aie {
namespace profile {
  void load()
  {
#ifdef XRT_ENABLE_AIE
    static xrt_core::module_loader xdp_aie_loader("xdp_aie_profile_plugin",
						    register_callbacks,
						    warning_callbacks);
#endif
  }
  std::function<void (void*, bool)> update_device_cb;
  std::function<void (void*)> end_poll_cb;

  void register_callbacks(void* handle)
  {
    using utype = void (*)(void*, bool);
    using ftype = void (*)(void*);
  
    update_device_cb =
      reinterpret_cast<utype>(xrt_core::dlsym(handle, "updateAIECtrDevice"));
    end_poll_cb =
      reinterpret_cast<ftype>(xrt_core::dlsym(handle, "endAIECtrPoll"));
  }

  void warning_callbacks()
  {
    // No warnings for AIE profiling
  }

} // end namespace profile

namespace ctr {
  void update_device(void* handle, bool hw_context_flow)
  {
    if (profile::update_device_cb != nullptr) {
      profile::update_device_cb(handle, hw_context_flow) ;
    }
  }

  void end_poll(void* handle)
  {
    if (profile::end_poll_cb != nullptr) {
      profile::end_poll_cb(handle) ;
    }
  }
} // end namespace ctr
} // end namespace xdp::aie
