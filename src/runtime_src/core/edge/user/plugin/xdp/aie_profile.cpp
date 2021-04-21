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

#include "aie_profile.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include <iostream>

namespace xdp {
namespace aie {
namespace profile {
  void load()
  {
#ifdef XRT_ENABLE_AIE
    static xrt_core::module_loader xdp_aie_loader("xdp_aie_profile_plugin",
						    register_callbacks,
						    warning_callbacks);
#endif
  }
  std::function<void (void*)> update_device_cb;
  std::function<void (void*)> end_poll_cb;

  void register_callbacks(void* handle)
  {
    typedef void (*ftype)(void*) ;

    update_device_cb = (ftype)(xrt_core::dlsym(handle, "updateAIECtrDevice")) ;
    if (xrt_core::dlerror() != NULL) update_device_cb = nullptr ;

    end_poll_cb = (ftype)(xrt_core::dlsym(handle, "endAIECtrPoll")) ;
    if (xrt_core::dlerror() != NULL) end_poll_cb = nullptr ;
  }

  void warning_callbacks()
  {
    // No warnings for AIE profiling
  }

} // end namespace profile

namespace ctr {
  void update_device(void* handle)
  {
    if (profile::update_device_cb != nullptr) {
      profile::update_device_cb(handle) ;
    }
  }

  void end_poll(void* handle)
  {
    if (profile::end_poll_cb != nullptr) {
      profile::end_poll_cb(handle) ;
    }
  }
} // end namespace ctr
} // end namespace aie
} // end namespace xdp
