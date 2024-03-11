// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
namespace xrt_core::detail {

namespace sfs = std::filesystem;

sfs::path
xilinx_xrt()
{
#if defined (__aarch64__) || defined (__arm__)
  return sfs::path("/usr");
#else
  return sfs::path("/opt/xilinx/xrt");
#endif
}

std::vector<sfs::path>
platform_repo_path()
{
  return {sfs::path("/lib/firmware/amdnpu"), sfs::path("/opt/xilinx/xrt/amdxdna")};
}

} // xrt_core::detail

