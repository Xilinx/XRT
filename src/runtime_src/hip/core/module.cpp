// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "module.h"

namespace xrt::core::hip {
module::
module(std::shared_ptr<context> ctx)
  : m_ctx{std::move(ctx)}
{}

// Global map of streams
xrt_core::handle_map<module_handle, std::shared_ptr<module>> module_cache;
}
