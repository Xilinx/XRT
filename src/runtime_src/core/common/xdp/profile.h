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
// that are used by the user's application.When called from the hardware
// context construction, it should pass true in the hw_context_flow variable
void 
update_device(void* handle, bool hw_context_flow);

// finish_flush_device should be called when the application ends or a hardware context
// is destroyed.  It is responsible for flushing out all of the device
// information from the device to host memory so it can be processed before
// the device is reset and the data is wiped.
void 
finish_flush_device(void* handle);

} // end namespace xrt_core::xdp

#endif
