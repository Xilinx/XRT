// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved

#ifndef CORE_COMMON_XDP_AIE_PROFILE_H
#define CORE_COMMON_XDP_AIE_PROFILE_H

// This file declares APIs for XDP AIE Profile Plugin which make 
// connections between all xrt_coreutil level hooks to the plugin.

namespace xrt_core::xdp::aie::profile {

void 
register_callbacks(void* handle);

void 
warning_callbacks();

void 
load();

// Make connections
void 
update_device(void* handle);

void 
end_poll(void* handle);

} // end namespace xrt_core::xdp::aie::profile

#endif // CORE_COMMON_XDP_AIE_PROFILE_H
