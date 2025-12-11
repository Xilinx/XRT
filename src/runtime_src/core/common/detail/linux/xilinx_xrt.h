// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/debug.h"
#include "core/common/module_loader.h"

#include <dlfcn.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef XRT_INSTALL_PREFIX
# error "XRT_INSTALL_PREFIX is undefined"
#endif

namespace xrt_core::detail {

namespace sfs = std::filesystem;

// Get XRT install path from DSO location or compile time constant
sfs::path
xilinx_xrt()
{
  Dl_info info {};
  if (dladdr(reinterpret_cast<void*>(&xilinx_xrt), &info) == 0 || !info.dli_fname)
    return sfs::path(XRT_INSTALL_PREFIX);

  if (std::string(info.dli_fname).find("libxrt_coreutil") == std::string::npos)
    return sfs::path(XRT_INSTALL_PREFIX);

  // Relocatable path based on install location of this DSO
  sfs::path so_path(sfs::canonical(info.dli_fname)); // /.../lib/libxrt_coreutil.so
  return so_path.parent_path().parent_path();
}

std::vector<sfs::path>
platform_repo_path()
{
  std::vector<sfs::path> paths;
  auto xrt = xrt_core::environment::xilinx_xrt();

  // 1. Install path
  // If xilinx_xrt is rooted at xrt, then share is subdir
  // otherwise xrt is rooted at share
  if (xrt.filename() == "xrt")
    // rooted at xrt, share is subdir
    paths.push_back(xrt / "share");
  else
    // rooted at non-xrt specific dir, append xrt/share
    paths.push_back(xrt / "share/xrt");

  // 2. XDG data path ($HOME/.local/share) is also a candidate
  if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
    paths.push_back(sfs::path(xdg) / "xrt");
  else if (const char* home = std::getenv("HOME"); home && *home)
    paths.push_back(sfs::path(home) / ".local/share/xrt");

  return paths;
}

} // xrt_core::detail

