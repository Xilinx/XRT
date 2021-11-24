/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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

#include "plugin/xdp/plugin_loader.h"
#include "plugin/xdp/hal_profile.h"
#include "core/common/module_loader.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/utils.h"
#include "core/common/dlfcn.h"

namespace xdp {
namespace hal {

std::function<void (bool, const char*, unsigned long long int)> generic_cb ;
std::function<void (bool, bool, const char*, unsigned long long int,
                    unsigned long long int,
                    unsigned long long int)> buffer_transfer_cb ;

  // The registration function
  void register_callbacks(void* handle)
  {
    using generic_type         = void (*)(bool, const char*,
                                          unsigned long long int) ;
    using buffer_transfer_type = void (*)(bool, bool, const char*,
                                          unsigned long long int,
                                          unsigned long long int,
                                          unsigned long long int) ;

    generic_cb =
      reinterpret_cast<generic_type>(xrt_core::dlsym(handle, "hal_generic_cb"));
    if (xrt_core::dlerror() != nullptr) generic_cb = nullptr ;

    buffer_transfer_cb =
      reinterpret_cast<buffer_transfer_type>(xrt_core::dlsym(handle, "buffer_transfer_cb")) ;
    if (xrt_core::dlerror() != nullptr) buffer_transfer_cb = nullptr ;
  }

  // The warning function
  void warning_callbacks()
  {
  }

  int error_function()
  {
    if (xrt_core::config::get_native_xrt_trace()) {
      xrt_core::message::send(xrt_core::message::severity_level::warning,
                              "XRT",
                              "Enabling both Native XRT and HAL level trace is not currently supported.  Only Native XRT tracing will be enabled.");
      return 1;
    }
    return 0;
  }

  void load()
  {
    static xrt_core::module_loader xdp_hal_loader("xdp_hal_plugin",
                                                  register_callbacks,
                                                  warning_callbacks,
                                                  error_function) ;
  }

  bool loader::hal_plugins_loaded = false ;
  loader::loader()
  {
    if (hal_plugins_loaded) return ;
    hal_plugins_loaded = true ;
    xdp::hal_hw_plugins::load() ;
  }

  api_call_logger::api_call_logger(const char* function)
    : m_id(0), m_fullname(function)
  {
  }

  generic_api_call_logger::generic_api_call_logger(const char* function)
    : api_call_logger(function)
  {
    if (generic_cb) {
      m_id = xrt_core::utils::issue_id() ;
      generic_cb(true, m_fullname, m_id) ;
    }
  }

  generic_api_call_logger::~generic_api_call_logger()
  {
    if (generic_cb) {
      generic_cb(false, m_fullname, m_id) ;
    }
  }

  buffer_transfer_logger::buffer_transfer_logger(const char* function, size_t size, bool isWrite)
    : api_call_logger(function), m_buffer_id(0), m_size(0), m_is_write(isWrite)
  {
    if (buffer_transfer_cb) {
      m_id        = xrt_core::utils::issue_id() ;
      m_buffer_id = xrt_core::utils::issue_id() ;
      m_size      = size ;

      buffer_transfer_cb(m_is_write, true, m_fullname, m_id, m_buffer_id, m_size) ;
    }
  }

  buffer_transfer_logger::~buffer_transfer_logger()
  {
    if (buffer_transfer_cb) {
      buffer_transfer_cb(m_is_write, false, m_fullname, m_id, m_buffer_id, m_size) ;
    }
  }

} // end namespace hal
} // end namespace xdp
