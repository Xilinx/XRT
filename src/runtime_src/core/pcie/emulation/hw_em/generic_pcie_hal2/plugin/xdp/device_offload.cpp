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

#include <functional>

#include "core/common/dlfcn.h"
#include "core/common/message.h"
#include "core/common/module_loader.h"
#include "core/common/utils.h"

#include "device_offload.h"

namespace xdp {
namespace hw_emu {
namespace device_offload {

  void load()
  {
    static xrt_core::module_loader
      xdp_device_offload_loader("xdp_hw_emu_device_offload_plugin",
                                register_callbacks,
                                warning_callbacks) ;
  }

  std::function<void (void*)> update_device_cb ;
  std::function<void (void*)> flush_device_cb ;

  void register_callbacks(void* handle)
  {
    using cb_type = void (*)(void*) ;

    update_device_cb =
      reinterpret_cast<cb_type>(xrt_core::dlsym(handle, "updateDeviceHWEmu")) ;
    if (xrt_core::dlerror() != nullptr)
      update_device_cb = nullptr ;

    flush_device_cb =
      reinterpret_cast<cb_type>(xrt_core::dlsym(handle, "flushDeviceHWEmu")) ;
    if (xrt_core::dlerror() != nullptr)
      flush_device_cb = nullptr ;
  }

  void warning_callbacks()
  {
    // No warnings
  }

} // end namespace device_offload

  void update_device(void* handle)
  {
    if (device_offload::update_device_cb != nullptr)
      device_offload::update_device_cb(handle) ;
  }

  void flush_device(void* handle)
  {
    if (device_offload::flush_device_cb != nullptr)
      device_offload::flush_device_cb(handle) ;
  }

} // end namespace hw_emu
} // end namespace xdp
