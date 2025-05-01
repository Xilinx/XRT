/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include "aie_status.h"

#include <iostream>

#include "core/common/dlfcn.h"
#include "core/common/module_loader.h"

namespace xdp {
namespace aie {
namespace status {
  void load()
  {
#ifdef XRT_ENABLE_AIE
    static xrt_core::module_loader xdp_aie_loader("xdp_aie_status_plugin",
                                                  register_callbacks,
                                                  warning_callbacks);
#endif
  }

  // Callback from shim to load device information and start polling
  std::function<void (void*, bool)> update_device_cb;
  // Callback from shim to end poll for a device when xclbin changes
  std::function<void (void*)> end_poll_cb;

  void register_callbacks(void* handle)
  {
    using utype = void (*)(void*, bool);
    using ftype = void (*)(void*);

    update_device_cb = reinterpret_cast<utype>(xrt_core::dlsym(handle, "updateAIEStatusDevice"));
    end_poll_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "endAIEStatusPoll"));
  }

  void warning_callbacks()
  {
    // No warnings for AIE status
  }

} // end namespace status

namespace sts {
  void update_device(void* handle, bool hw_context_flow)
  {
    if (status::update_device_cb != nullptr)
      status::update_device_cb(handle, hw_context_flow);
  }

  void end_poll(void* handle)
  {
    if (status::end_poll_cb != nullptr)
      status::end_poll_cb(handle);
  }
} // end namespace sts
} // end namespace aie
} // end namespace xdp
