// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#include "trace.h"
#include "detail/trace.h"

#include <memory>
#include <thread>

namespace xrt_core {

trace*
get_trace()
{
  static thread_local auto trace_object = detail::trace::create_trace_object();
  return trace_object.get();
}

} // xrt_core
