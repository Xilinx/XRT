// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef xbtracer_h
#define xbtracer_h
#include <string>
#include <vector>

#ifdef _WIN32
inline constexpr const char* WRAPPER_LIB = "xrt_wrapper.dll";
#else
inline constexpr const char* WRAPPER_LIB = "libxrt_wrapper.so";
#endif

namespace xrt::tools::xbtracer {
struct tracer_arg {
  bool verbose;
  std::vector<std::string> target_app;
  std::string out_dir;
};

int launch_app(const struct tracer_arg &arg);
} // namespace xrt::tools::xbtracer
#endif // xbtracer_h
