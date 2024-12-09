// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_uuid.h"

#include "core/common/cuidx_type.h"
#include <memory>

// This file defines APIs for compute unit (ip) context management
// It is used by xrt::kernel and xrt::ip implementation.

namespace xrt_core {

class device;

// Context management is somewhat complex in multi-threaded host
// applications where same device object is shared between threads.
//
// A call to xclOpenContext must be protected against a race where
// one thread is in the process of call xclCloseContext.
namespace context_mgr {

class device_context_mgr;

// Create a context manager for a specific device The manager is
// shared and cached so that it is constructed only if necessary.  In
// other words, multi threads using same device can share the same
// context manager.
std::shared_ptr<device_context_mgr>
create(const xrt_core::device* device);

// Open a device context a specified compute unit (ip)
//
// @hwctx:  hardware context in which the IP should be opened
// @ipname: name of IP to open
// @Return: the index of the IP as cuidx_type.
//
// The function blocks until the context can be acquired.  If
// timeout, then the function throws.
//
// Note that the context manager is not intended to support two or
// more threads opening a context on the same compute unit. This
// situation must be guarded by the client (xrt::kernel) of the
// manager.
//
// The function is simply a synchronization between two threads
// simultanous use of open_context and close_context.
cuidx_type
open_context(const xrt::hw_context& hwctx, const std::string& ipname);

// Close a previously opened device context
//
// @hwctx:  hardware context that has the CU opened
// @cuidx:  index of CU
//
// The function throws if no context is open on specified CU.
void
close_context(const xrt::hw_context& hwctx, cuidx_type cuidx);

}} // context_mgr, xrt_core
