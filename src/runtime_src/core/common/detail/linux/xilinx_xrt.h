// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef XRT_INSTALL_PREFIX
# error "XRT_INSTALL_PREFIX is undefined"
#endif

namespace xrt_core::detail {

namespace sfs = std::filesystem;

sfs::path
xilinx_xrt()
{
  // This returns CMAKE_INSTALL_PREFIX.  The internal default cmake
  // install path is /opt/xilinx/xrt, for upstreaming most likely /usr.
  return sfs::path(XRT_INSTALL_PREFIX);
}

std::vector<sfs::path>
platform_repo_path()
{
  // If CMAKE_INSTALL_PREFIX is /usr, then /usr/amdxdna will be invalid,
  // not sure if this matters or why the second path is even present?
  // Regardless, default /opt/xilinx/xrt behavior remains unchanged.
  return {sfs::path("/lib/firmware/amdnpu"), xilinx_xrt() / "amdxdna"};
}

} // xrt_core::detail

