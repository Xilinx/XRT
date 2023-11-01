// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#include <filesystem>
#include <stdexcept>
#include <string>
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

sfs::path
xclbin_repo_path()
{
  // current directory
  return sfs::current_path();
}

} // xrt_core::detail
