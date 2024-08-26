/**
 * Copyright (C) 2020 Xilinx, Inc
 * Copyright (C) 2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include "hal_device_offload.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"

namespace xdp {
namespace hal {
namespace device_offload {

  void load()
  {
    static xrt_core::module_loader
      xdp_hal_device_offload_loader("xdp_hal_device_offload_plugin",
                                    register_functions,
                                    warning_function,
                                    error_function);
  }

  std::function<void (void*)> update_device_cb ;
  std::function<void (void*)> flush_device_cb ;
 
  void register_functions(void* handle)
  {
    typedef void (*ftype)(void*) ;
    update_device_cb = (ftype)(xrt_core::dlsym(handle, "updateDeviceHAL")) ;
    if (xrt_core::dlerror() != NULL) update_device_cb = nullptr ;

    flush_device_cb = (ftype)(xrt_core::dlsym(handle, "flushDeviceHAL")) ;
    if (xrt_core::dlerror() != NULL) flush_device_cb = nullptr ;
  }

  void warning_function()
  {
    // No warnings at this level
  }

  int error_function()
  {
    return 0 ;
  }

} // end namespace device_offload

  void flush_device(void* handle)
  {
    if (device_offload::flush_device_cb != nullptr)
    {
      device_offload::flush_device_cb(handle) ;
    }
  }

  void update_device(void* handle)
  {
    if (device_offload::update_device_cb != nullptr)
    {
      device_offload::update_device_cb(handle) ;
    }
  }
} // end namespace hal
} // end namespace xdp

