// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// This file is an implementation detail of the XRT coreutil library.
// It is not part of the public XRT API. Include guards are not
// at this level.
#include "core/common/debug.h"
#include "core/common/module_loader.h"

#include <dlfcn.h>

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef XRT_INSTALL_PREFIX
# error "XRT_INSTALL_PREFIX is undefined"
#endif

#ifndef XRT_VERSION_STRING
# error "XRT_VERSION_STRING is undefined"
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

  try {
    // Relocatable path based on install location of this DSO
    sfs::path so_path(sfs::canonical(info.dli_fname)); // /.../lib/libxrt_coreutil.so
    return so_path.parent_path().parent_path();
  }
  catch (const std::exception&) {
    return sfs::path(XRT_INSTALL_PREFIX);
  }
}

// platform_repo_path() - Get candidate paths for platform repository data
//
// Returns a prioritized list of filesystem paths where
// platform-specific data files (e.g., FPGA platform metadata) may be
// located. The search order is:
//
// 1. XRT installation share directory:
//    - <xrt_root>/share (if xrt_root ends with "xrt")
//    - <xrt_root>/share/xrt (otherwise)
//
// 2. XDG user data directory (if XDG_DATA_HOME is set):
//    - $XDG_DATA_HOME/xrt/<version>
//    - $XDG_DATA_HOME/xrt
//
// 3. User's local share directory (if HOME is set):
//    - $HOME/.local/share/xrt/<version>
//    - $HOME/.local/share/xrt
//
// Returned paths are not validated - caller must check existence
// Follows XDG Base Directory Specification for user data paths
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
  if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) {
    paths.push_back(sfs::path(xdg) / "xrt" / XRT_VERSION_STRING);
    paths.push_back(sfs::path(xdg) / "xrt");
  }
  else if (const char* home = std::getenv("HOME"); home && *home) {
    paths.push_back(sfs::path(home) / ".local/share/xrt" / XRT_VERSION_STRING);
    paths.push_back(sfs::path(home) / ".local/share/xrt");
  }

  return paths;
}

} // xrt_core::detail

