// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_module_h
#define xrthip_module_h

#include "context.h"

namespace xrt::core::hip {
/**
 * typedef module_handle - opaque module handle
 */
typedef void* module_handle;

class module
{
  std::shared_ptr<context> m_ctx;

public:
  module(std::shared_ptr<context> ctx);
};

extern xrt_core::handle_map<module_handle, std::shared_ptr<module>> module_cache;
} // xrt::core::hip

#endif
