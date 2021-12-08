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

#include "pl_deadlock.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include <iostream>

namespace xdp {
namespace pl_deadlock {
namespace profile {
  void load()
  {
    static xrt_core::module_loader xdp_pl_deadlock_loader("xdp_pl_deadlock_plugin",
                register_callbacks,
                warning_callbacks);
  }
  std::function<void (void*)> update_device_cb;

  void register_callbacks(void* handle)
  {
    typedef void (*ftype)(void*);

    update_device_cb = (ftype)(xrt_core::dlsym(handle, "updateDevicePLDeadlock"));
    if (xrt_core::dlerror() != NULL)
      update_device_cb = nullptr;
  }

  void warning_callbacks()
  {
    // No warnings for PL Deadlock Detection
  }

} // end namespace profile

  void update_device(void* handle)
  {
    if (profile::update_device_cb != nullptr)
      profile::update_device_cb(handle);
  }

} // end namespace pl_deadlock
} // end namespace xdp
