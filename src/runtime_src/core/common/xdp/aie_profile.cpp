// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved

#define XRT_CORE_COMMON_SOURCE
#include "core/common/xdp/aie_profile.h"

#include "core/common/dlfcn.h"
#include "core/common/module_loader.h"

#include <functional>
// This file defines APIs for XDP AIE Profile Plugin which make 
// connections between all xrt_coreutil level hooks to the plugin.
// It is responsible for loading all of modules.

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

