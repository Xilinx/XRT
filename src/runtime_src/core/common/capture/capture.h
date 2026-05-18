
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_CAPTURE_CAPTURE_H_
#define XRT_COMMON_CAPTURE_CAPTURE_H_
#include "xrt/detail/config.h"
#include "core/common/config_reader.h"
#include "core/common/capture/fn_fwd.h"

namespace xrt_core::capture {

size_t
num_frames();

inline bool
is_enabled()
{
  static auto frames = xrt_core::config::get_capture_frames();
  if (!frames)
    return false;

  return num_frames() < frames;
}

// No-op when disabled, actual capture when enabled
#define XRT_REPLAY_CAPTURE(fn, ...) \
  do { if (XRT_UNLIKELY(xrt_core::capture::is_enabled())) { \
      xrt_core::capture::fn(__VA_ARGS__);           \
    } } while(0)

} // namespace xrt_core::capture

#endif
