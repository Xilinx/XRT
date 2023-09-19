// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
#ifndef CORE_COMMON_PROFILE_DOT_H
#define CORE_COMMON_PROFILE_DOT_H

// The functions here are the general interfaces for the XDP hooks that are
// called from the common coreutil library and not the specific shims.
namespace xrt_core::xdp {

// update_device should be called whenever a user creates a hardware context.
// This hook will allow the XDP plugins to cache a reference to the user's
// hardware context so the plugin can configure and read performance counters
// that are used by the user's application.
void 
update_device(void* handle);

// end_poll should be called when the application ends or a hardware context
// is destroyed.  It is responsible for flushing out all of the device
// information from the device to host memory so it can be processed before
// the device is reset and the data is wiped.
void 
end_poll(void* handle);

} // end namespace xrt_core::xdp

namespace xrt_core::xdp::aie::profile {

void load();
void register_callbacks(void* handle);
void warning_callbacks();

void update_device(void* handle);
void end_poll(void* handle);

} // end namespace xrt_core::xdp::aie::profile

#endif