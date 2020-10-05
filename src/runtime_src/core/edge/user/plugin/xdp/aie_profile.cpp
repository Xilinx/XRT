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

namespace xdpaieprofile {

  void load_xdp_aie_plugin()
  {
#ifdef XRT_ENABLE_AIE
    static xrt_core::module_loader xdp_aie_loader("xdp_aie_plugin",
						    register_aie_callbacks,
						    warning_aie_callbacks);
#endif
  }
  std::function<void (void*)> update_aie_device_cb;
  std::function<void (void*)> end_aie_ctr_poll_cb;

  void register_aie_callbacks(void* handle)
  {
    typedef void (*ftype)(void*) ;

    update_aie_device_cb = (ftype)(xrt_core::dlsym(handle, "updateAIECtrDevice")) ;
    if (xrt_core::dlerror() != NULL) update_aie_device_cb = nullptr ;

    end_aie_ctr_poll_cb = (ftype)(xrt_core::dlsym(handle, "endAIECtrPoll")) ;
    if (xrt_core::dlerror() != NULL) end_aie_ctr_poll_cb = nullptr ;
  }

  void warning_aie_callbacks()
  {
    // No warnings for AIE profiling
  }

} // end namespace xdpaieprofile

namespace xdpaiectr {

  void update_aie_device(void* handle)
  {
    if (xdpaieprofile::update_aie_device_cb != nullptr) {
      xdpaieprofile::update_aie_device_cb(handle) ;
    }
  }

  void end_aie_ctr_poll(void* handle)
  {
    if (xdpaieprofile::end_aie_ctr_poll_cb != nullptr) {
      xdpaieprofile::end_aie_ctr_poll_cb(handle) ;
    }
  }
}
