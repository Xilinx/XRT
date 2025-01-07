// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. - All rights reserved
#define XRT_CORE_COMMON_SOURCE
#include "core/common/xdp/profile.h"

#include "core/common/config_reader.h"
#include "core/common/dlfcn.h"
#include "core/common/module_loader.h"
#include <functional>

#ifdef _WIN32
#pragma warning( disable : 4996 ) /* Disable warning for getenv */
#endif

// This file makes the connections between all xrt_coreutil level hooks
// to the corresponding xdp plugins.  It is responsible for loading all of
// modules.

namespace xrt_core::xdp::core {
  void
  register_callbacks(void*)
  {}

  void
  warning_callbacks()
  {}

  void
  load_core()
  {
    static xrt_core::module_loader xdp_core_loader("xdp_core",
                                                   register_callbacks,
                                                   warning_callbacks);
  }
}

namespace xrt_core::xdp::aie::profile {

std::function<void (void*)> update_device_cb;
std::function<void (void*)> end_poll_cb;

void 
register_callbacks(void* handle)
{  
  #ifdef XDP_CLIENT_BUILD
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
  #ifdef XDP_CLIENT_BUILD
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


namespace xrt_core::xdp::ml_timeline {

std::function<void (void*)> update_device_cb;
std::function<void (void*)> finish_flush_device_cb;

void
register_callbacks(void* handle)
{ 
  #if defined(XDP_CLIENT_BUILD) || defined(XDP_VE2_BUILD)
    using ftype = void (*)(void*);

    update_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "updateDeviceMLTmln"));
    finish_flush_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "finishflushDeviceMLTmln"));

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
  static xrt_core::module_loader xdp_loader("xdp_ml_timeline_plugin",
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
finish_flush_device(void* handle)
{
  if (finish_flush_device_cb)
    finish_flush_device_cb(handle);
}

} // end namespace xrt_core::xdp::ml_timeline

namespace xrt_core::xdp::aie_pc {

std::function<void (void*)> update_device_cb;
std::function<void (void*)> finish_flush_device_cb;

void
register_callbacks(void* handle)
{ 
  #ifdef XDP_CLIENT_BUILD
    using ftype = void (*)(void*);

    update_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "updateDeviceAIEPC"));
    finish_flush_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "finishflushDeviceAIEPC"));
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
  static xrt_core::module_loader xdp_loader("xdp_aie_pc_plugin",
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
finish_flush_device(void* handle)
{
  if (finish_flush_device_cb)
    finish_flush_device_cb(handle);
}

} // end namespace xrt_core::xdp::aie_pc

namespace xrt_core::xdp::pl_deadlock {

std::function<void (void*)> update_device_cb;
std::function<void (void*)> finish_flush_device_cb;

void
register_callbacks(void* handle)
{ 
  #ifdef XDP_CLIENT_BUILD
    (void)handle;	// Not supported on Client Devices.
  #else
    using ftype = void (*)(void*);

    update_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "updateDevicePLDeadlock"));
    finish_flush_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "flushDevicePLDeadlock"));
  #endif

}

void
warning_callbacks()
{
}

void
load()
{
  static xrt_core::module_loader xdp_loader("xdp_pl_deadlock_plugin",
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
finish_flush_device(void* handle)
{
  if (finish_flush_device_cb)
    finish_flush_device_cb(handle);
}

} // end namespace xrt_core::xdp::pl_deadlock

namespace xrt_core::xdp::aie::trace {

std::function<void (void*)> update_device_cb;
std::function<void (void*)> end_trace_cb;


void 
register_callbacks(void* handle)
{  
  #ifdef XDP_CLIENT_BUILD
    using ftype = void (*)(void*);

    end_trace_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "finishFlushAIEDevice"));
    update_device_cb = reinterpret_cast<ftype>(xrt_core::dlsym(handle, "updateAIEDevice"));
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
  static xrt_core::module_loader xdp_aie_trace_loader("xdp_aie_trace_plugin",
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
end_trace(void* handle)
{
  if (end_trace_cb)
    end_trace_cb(handle);
}

} // end namespace xrt_core::xdp::aie::trace

namespace xrt_core::xdp::aie::halt {

std::function<void (void*)> update_device_cb;
std::function<void (void*)> finish_flush_device_cb;

void
register_callbacks(void* handle)
{
  #ifdef XDP_CLIENT_BUILD
    update_device_cb = reinterpret_cast<void (*)(void*)>(xrt_core::dlsym(handle, "updateDeviceAIEHalt"));
    finish_flush_device_cb = reinterpret_cast<void (*)(void*)>(xrt_core::dlsym(handle, "finishFlushDeviceAIEHalt"));
  #else
    (void)handle;
  #endif
}

void
warning_callbacks()
{}

void
load()
{
  static xrt_core::module_loader xdp_aie_halt_loader("xdp_aie_halt_plugin",
                                                     register_callbacks,
                                                     warning_callbacks);
}

void
update_device(void* handle)
{
  if (update_device_cb)
    update_device_cb(handle);
}

void
finish_flush_device(void* handle)
{
  if (finish_flush_device_cb)
    finish_flush_device_cb(handle);
}

} // end namespace xrt_core::xdp::aie::halt

namespace xrt_core::xdp {

void 
update_device(void* handle)
{

#ifdef XDP_CLIENT_BUILD
  /* Adding the macro guard as the static instances of the following plugins
   * get created unnecessarily when the configs are enabled on Edge.
   */
  #ifdef _WIN32
  if (xrt_core::config::get_ml_timeline()
      || xrt_core::config::get_aie_profile()
      || xrt_core::config::get_aie_trace()
      || xrt_core::config::get_aie_debug()
      || xrt_core::config::get_aie_halt()
      || xrt_core::config::get_aie_pc()) {
    /* All the above plugins are dependent on xdp_core library. So,
     * explicitly load it to avoid library search issue in implicit loading.
     */
    try {
      xrt_core::xdp::core::load_core();
    } catch (...) {
      return;
    }
  }
  #endif

  if (xrt_core::config::get_aie_halt()) {
    try {
      xrt_core::xdp::aie::halt::load();
    }
    catch (...) {
      return;
    }
    xrt_core::xdp::aie::halt::update_device(handle);
  }

  if (xrt_core::config::get_aie_profile()) {
    try {
      xrt_core::xdp::aie::profile::load();
    } 
    catch (...) {
      return;
    }
    xrt_core::xdp::aie::profile::update_device(handle);
  }

  if (xrt_core::config::get_aie_trace()) {
    try {
      xrt_core::xdp::aie::trace::load();
    } 
    catch (...) {
      return;
    }
    xrt_core::xdp::aie::trace::update_device(handle);
    
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

  if (xrt_core::config::get_ml_timeline()) {
    try {
      xrt_core::xdp::ml_timeline::load();
    }
    catch (...) {
      return;
    }
    xrt_core::xdp::ml_timeline::update_device(handle);
  }

  if (xrt_core::config::get_aie_pc()) {
    try {
      xrt_core::xdp::aie_pc::load();
    }
    catch (...) {
      return;
    }
    xrt_core::xdp::aie_pc::update_device(handle);
  }

#elif defined(XDP_VE2_BUILD)

  if (xrt_core::config::get_ml_timeline()) {
    try {
      xrt_core::xdp::ml_timeline::load();
    }
    catch (...) {
      return;
    }
    xrt_core::xdp::ml_timeline::update_device(handle);
  }

#else

  if (xrt_core::config::get_pl_deadlock_detection() 
      && nullptr == std::getenv("XCL_EMULATION_MODE")) {
    try {
      xrt_core::xdp::pl_deadlock::load();
    }
    catch (...) {
      return;
    }
    xrt_core::xdp::pl_deadlock::update_device(handle);
  }
#endif
}

void 
finish_flush_device(void* handle)
{

#ifdef XDP_CLIENT_BUILD

  if (xrt_core::config::get_aie_halt())
    xrt_core::xdp::aie::halt::finish_flush_device(handle);
  if (xrt_core::config::get_aie_profile())
    xrt_core::xdp::aie::profile::end_poll(handle);
  if (xrt_core::config::get_aie_trace())
    xrt_core::xdp::aie::trace::end_trace(handle);
  if (xrt_core::config::get_aie_debug())
    xrt_core::xdp::aie::debug::end_debug(handle);
  if (xrt_core::config::get_ml_timeline())
    xrt_core::xdp::ml_timeline::finish_flush_device(handle);
  if (xrt_core::config::get_aie_pc())
    xrt_core::xdp::aie_pc::finish_flush_device(handle);

#elif defined(XDP_VE2_BUILD)

  if (xrt_core::config::get_ml_timeline())
    xrt_core::xdp::ml_timeline::finish_flush_device(handle);

#else

  if (xrt_core::config::get_pl_deadlock_detection()
      && nullptr == std::getenv("XCL_EMULATION_MODE")) {
    xrt_core::xdp::pl_deadlock::finish_flush_device(handle);
  }
#endif
}

} // end namespace xrt_core::xdp

