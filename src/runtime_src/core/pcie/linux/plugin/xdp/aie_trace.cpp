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

#include <functional>
#include "aie_trace.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"

namespace xdp::aie {
namespace trace {

  void load()
  {
    static xrt_core::module_loader xdp_aie_trace_loader("xdp_aie_trace_plugin",
                                                        register_callbacks,
                                                        warning_function,
                                                        error_function);
  }

  std::function<void (void*, bool)> update_device_cb;
  std::function<void (void*)> flush_device_cb;
  std::function<void (void*)> finish_flush_device_cb;

  void register_callbacks(void* handle)
  {
    using utype = void (*)(void*, bool);
    using ftype = void (*)(void*);

    update_device_cb =
       reinterpret_cast<utype>(xrt_core::dlsym(handle, "updateAIEDevice"));
    flush_device_cb =
      reinterpret_cast<ftype>(xrt_core::dlsym(handle, "flushAIEDevice"));
    finish_flush_device_cb =
      reinterpret_cast<ftype>(xrt_core::dlsym(handle, "finishFlushAIEDevice"));
  }

  void warning_function()
  {
  }

  int error_function()
  {
    return 0 ;
  }

} // end namespace trace

  void update_device(void* handle, bool hw_context_flow)
  {
    if (trace::update_device_cb != nullptr) {
      trace::update_device_cb(handle, hw_context_flow) ;
    }
  }

  void flush_device(void* handle)
  {
    if (trace::flush_device_cb != nullptr) {
      trace::flush_device_cb(handle) ;
    }
  }

  void finish_flush_device(void* handle)
  {
    if (trace::finish_flush_device_cb != nullptr) {
      trace::finish_flush_device_cb(handle) ;
    }
  }

} // end namespace xdp::aie
