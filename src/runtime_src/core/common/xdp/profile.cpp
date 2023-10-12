// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
#define XRT_CORE_COMMON_SOURCE
#include "core/common/xdp/profile.h"

#include "core/common/config_reader.h"
#include "core/common/dlfcn.h"
#include "core/common/module_loader.h"
#include <functional>

// This file makes the connections between all xrt_coreutil level hooks
// to the corresponding xdp plugins.  It is responsible for loading all of
// modules.

namespace xrt_core::xdp::aie::profile {

std::function<void (void*)> update_device_cb;
std::function<void (void*)> end_poll_cb;

void 
register_callbacks(void* handle)
{  
  #ifdef XDP_MINIMAL_BUILD
    using ftype = void (*)(void*);

    update_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "updateAIECtrDevice"));
    end_poll_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "endAIECtrPoll"));
  #else 
    (void)handle;
  #endif

}

void 
warning_callbacks()
{
}

void 
load()
{
  static xrt_core::module_loader xdp_aie_loader("xdp_aie_profile_plugin",
                                                register_callbacks,
                                                warning_callbacks);
}

// Make connections
void 
update_device(void* handle)
{
  if (update_device_cb)
    update_device_cb(handle);
}

void 
end_poll(void* handle)
{
  if (end_poll_cb)
    end_poll_cb(handle);
}

} // end namespace xrt_core::xdp::aie::profile

namespace xrt_core::xdp::aie::debug {

std::function<void (void*)> update_device_cb;
std::function<void (void*)> end_debug_cb;

void 
register_callbacks(void* handle)
{  
  #ifdef XDP_MINIMAL_BUILD
    using ftype = void (*)(void*);

    end_debug_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "endAIEDebugRead"));
    update_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "updateAIEDebugDevice"));
  #else 
    (void)handle;
  #endif
}

void 
warning_callbacks()
{
}

void 
load()
{
  static xrt_core::module_loader xdp_aie_debug_loader("xdp_aie_debug_plugin",
                                                register_callbacks,
                                                warning_callbacks);
}

// Make connections
void 
update_device(void* handle)
{
  if (update_device_cb)
    update_device_cb(handle);
}

void 
end_debug(void* handle)
{
  if (end_debug_cb)
    end_debug_cb(handle);
}

} // end namespace xrt_core::xdp::aie::debug

namespace xrt_core::xdp {

void 
update_device(void* handle)
{
  if (xrt_core::config::get_aie_profile()) {
    try {
      xrt_core::xdp::aie::profile::load();
    } 
    catch (...) {
      return;
    }
    xrt_core::xdp::aie::profile::update_device(handle);
  }

  if (xrt_core::config::get_aie_debug()) {
    try {
      xrt_core::xdp::aie::debug::load();
    } 
    catch (...) {
      return;
    }
    xrt_core::xdp::aie::debug::update_device(handle);
  }
}

void 
finish_flush_device(void* handle)
{
  if (xrt_core::config::get_aie_profile())
    xrt_core::xdp::aie::profile::end_poll(handle);

  if (xrt_core::config::get_aie_debug())
    xrt_core::xdp::aie::debug::end_debug(handle);
}

} // end namespace xrt_core::xdp

