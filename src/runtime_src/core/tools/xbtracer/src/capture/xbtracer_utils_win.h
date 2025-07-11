// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef xbtracer_utils_win_h
#define xbtracer_utils_win_h

#include <string>
#include <tuple>
#include <vector>

namespace xrt::tools::xbtracer {

int copy_libs_to_temp(std::string &temp_path,
                      const std::vector<std::tuple<std::string, std::string>> &libs);
}

#endif // xbtracer_utils_win_h
