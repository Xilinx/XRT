// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

// This file is an implementation detail of the XRT coreutil library.
// It is not part of the public XRT API. Include guards are not
// at this level.
#include "core/common/debug.h"
#include "core/common/module_loader.h"
#include "core/common/utils.h"

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

// Get XRT install path from DSO location or compile time constant.
//
// For non-elevated processes, use dladdr() to find the directory that
// the dynamic linker actually loaded libxrt_coreutil.so from.  This
// allows developers to work with multiple XRT installations side-by-side:
// whichever libxrt_coreutil.so the application resolved to (via
// LD_LIBRARY_PATH, RPATH, etc.) is the one whose sibling libxrt_core.so
// will be loaded (AIESW-32875).
//
// For elevated processes (geteuid()==0 / sudo), dladdr() would reflect
// wherever LD_LIBRARY_PATH points, which an attacker could control.
// We fall back to the compile-time XRT_INSTALL_PREFIX in that case to
// prevent untrusted-search-path injection (SWSPLAT-24084 / CWE-426).
sfs::path
xilinx_xrt()
{
  // Safety valve: elevated processes must use the compile-time path.
  if (!xrt_core::utils::is_elevated_process()) {
    // Use a symbol defined in this translation unit so dladdr() always
    // resolves to libxrt_coreutil.so regardless of which .so defines it.
    Dl_info info = {};
    if (::dladdr(reinterpret_cast<const void*>(&xilinx_xrt), &info) != 0
        && info.dli_fname && *info.dli_fname) {
      sfs::path coreutil_dir = sfs::path(info.dli_fname).parent_path();
      if (!coreutil_dir.empty())
        return coreutil_dir;
    }
  }

  // Elevated process or dladdr() failed: use compile-time install prefix.
  // This returns CMAKE_INSTALL_PREFIX.  The internal default cmake
  // install path is /opt/xilinx/xrt, for upstreaming most likely /usr.
  // In relocatable installs XILINX_XRT should be set (for non-elevated
  // processes) and this function will not normally be reached.
  return sfs::path(XRT_INSTALL_PREFIX);
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

