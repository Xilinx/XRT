/**
 * Copyright (C) 2020 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#include "plugin/xdp/aie_profile.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"

namespace xdp {
namespace aie {
namespace profile {

  void load()
  {
    static xrt_core::module_loader xdp_aie_loader("xdp_aie_profile_plugin",
						    register_callbacks,
						    warning_callbacks);
  }

  std::function<void (void*)> update_device_cb;
  std::function<void (void*)> end_poll_cb;

  void register_callbacks(void* handle)
  {
    using ftype = void (*)(void*); // Device handle

    update_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "updateAIECtrDevice"));
    end_poll_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "endAIECtrPoll"));
  
  }

  void warning_callbacks()
  {
    // No warnings for AIE profiling
  }

} // end namespace profile

namespace ctr {
  void update_device(void* handle)
  {
    if (profile::update_device_cb != nullptr)
      profile::update_device_cb(handle);
    
  }

  void end_poll(void* handle)
  {
    if (profile::end_poll_cb != nullptr)
      profile::end_poll_cb(handle);
    
  }
} // end namespace ctr
} // end namespace aie
} // end namespace xdp
