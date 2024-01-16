// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_stream_h
#define xrthip_stream_h

#include "context.h"

namespace xrt::core::hip {
/**
 * typedef stream_handle - opaque stream handle
 */
typedef void* stream_handle;

class stream
{
  std::shared_ptr<context> m_ctx;

public:
  stream(std::shared_ptr<context> ctx);
};

extern xrt_core::handle_map<stream_handle, std::shared_ptr<stream>> stream_cache;
} // xrt::core::hip

#endif
