// PDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_API_HW_CONTEXT_INT_H
#define XRT_COMMON_API_HW_CONTEXT_INT_H

#include "core/common/config.h"

// This file defines implementation extensions to the XRT XCLBIN APIs.
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/experimental/xrt_module.h"

#include <cstdint>

// Provide access to xrt::xclbin data that is not directly exposed
// to end users via xrt::xclbin.   These functions are used by
// XRT core implementation.
namespace xrt_core { namespace hw_context_int {

// Get the core_device from this context
// Exported for xdp access
XRT_CORE_COMMON_EXPORT
std::shared_ptr<xrt_core::device>
get_core_device(const xrt::hw_context& ctx);

// Get a raw pointer to the core device associated with
// the hw context
xrt_core::device*
get_core_device_raw(const xrt::hw_context& ctx);

// Backdoor for changing qos of a hardware context after it has
// been constructed.  The new qos affects how compute units are
// within the context are opened.  This is used for legacy
// xrt::kernel objects associated with a mailbox
void
set_exclusive(xrt::hw_context& ctx);

// Allows the creation of the hardware context from a void pointer
// to the hardware context implementation. We use a void pointer
// because we need to dynamically link to the callbacks that exist in 
// XDP with a C-style interface. Additionally, we do not want to 
// expose the hardware_context implementation class. This is used by
// XDP plugins in order to initialize when the user creates
// a hardware context in their host
XRT_CORE_COMMON_EXPORT 
xrt::hw_context
create_hw_context_from_implementation(void* hwctx_impl);

// Checks all the modules that are registered with given hw context
// and returns the module with the given kernel name
// throws if no module is found with given kernel name
xrt::module
get_module(const xrt::hw_context& hwctx, const std::string& kname);

}} // hw_context_int, xrt_core

#endif
