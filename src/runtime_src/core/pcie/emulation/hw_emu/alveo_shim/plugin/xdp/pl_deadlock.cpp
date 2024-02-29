/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

/*
 * Plugin for loading hw emulation xdp plugin for PL Deadlock Detection
 * The xdp plugin is used to update run summary with deadlock diagnosis
 * information. The diagnosis file comes from simulation and is written
 * automatically when a deadlock is detected.
 */

#include "core/common/dlfcn.h"
#include "core/common/module_loader.h"

#include "pl_deadlock.h"

#include <iostream>

namespace xdp::hw_emu::pl_deadlock {

  void load()
  {
    static xrt_core::module_loader xdp_pl_deadlock_loader("xdp_hw_emu_pl_deadlock_plugin",
                register_callbacks,
                warning_callbacks);
  }
  std::function<void (void*)> update_device_cb;

  void register_callbacks(void* handle)
  {
    using cb_type = void (*)(void*);

    update_device_cb =
      reinterpret_cast<cb_type>(xrt_core::dlsym(handle, "updateDevicePLDeadlock"));
    if (xrt_core::dlerror() != NULL)
      update_device_cb = nullptr;
  }

  void warning_callbacks()
  {
    // No warnings for PL Deadlock Detection
  }

  void update_device(void* handle)
  {
    if (update_device_cb != nullptr)
      update_device_cb(handle);
  }

} // end namespace xdp::hw_emu::pl_deadlock
