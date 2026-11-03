// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.

// This file is an implementation detail of the XRT coreutil library.
// It is not part of the public XRT API. Include guards are not
// at this level.
//
// Mirrors detail/windows/xilinx_xrt.h's D3DKMTXxx adapter access, but for
// the WSL2/GPU-PV path: D3DKMTEnumAdapters3/D3DKMTQueryAdapterInfo/
// D3DKMTCloseAdapter are defined by libdxg (vendor/libdxg/src/
// d3dkmt-wsl.cpp), compiled directly into this DSO via the
// dxg_objects_coreutil object library (see cmake/libdxg.cmake,
// cmake/xrt.cmake) — so, same as the umd/wsl UMD shim, they are called
// here as ordinary linked functions. No GetProcAddress/dlsym equivalent is
// needed or used.
//
// xilinx_xrt()/platform_repo_path() are the two functions module_loader.cpp
// actually requires of every detail/<platform>/xilinx_xrt.h (confirmed
// against detail/linux/xilinx_xrt.h, the file WSL used before this one
// existed — WSL is not _WIN32, so it fell through to the generic Linux
// path). detail/windows/xilinx_xrt.h additionally defines its own
// system_xilinx_xrt() helper, but that is Windows-internal plumbing (used
// only by that file's own xilinx_xrt() to resolve Windows' Driver Store
// path), not part of the shared contract — so it is deliberately not
// reproduced here. Windows uses D3DKMTQueryAdapterInfo's
// KMTQAITYPE_QUERYREGISTRY/D3DDDI_QUERYREGISTRY_DRIVERSTOREPATH escape to
// ask the driver where its Driver Store installed the matching driver
// package, a Windows installer concept with no WSL/Linux equivalent — XRT
// here is installed via the build's own CMAKE_INSTALL_PREFIX/apt package,
// not a signed driver package. So xilinx_xrt() below instead reuses the
// same dladdr()-based, driver-agnostic approach as
// detail/linux/xilinx_xrt.h verbatim.
//
// The `adapter`/`adapter_list` helpers below exist as scaffolding for
// whatever NPU-adapter-specific queries (beyond path discovery) need to
// be added here — they are not currently called by xilinx_xrt()/
// platform_repo_path().

#include "core/common/debug.h"
#include "core/common/module_loader.h"
#include "core/common/utils.h"

// wsl/winadapter.h must precede dxg/d3dkmthk.h — it supplies the base
// Windows types (ULONG, PVOID, DISPATCH_LEVEL, ...) the dxg headers build
// on. umd/wsl production code gets this ordering transitively via
// umd.h -> detail/linux/platform.h; this file has no such dependency, so
// it is spelled out explicitly here instead.
#include <wsl/winadapter.h>
#include <dxg/d3dkmthk.h>

#include <dlfcn.h>

#include <cstdlib>
#include <cstring>
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

// XRT_LIB_DIR (CMAKE_INSTALL_LIBDIR) is supplied as a compile definition on
// module_loader.cpp, the only translation unit that includes this header.
// This header is not meant to be compiled stand-alone, so error out if it is
// missing rather than guessing a default.
#ifndef XRT_LIB_DIR
# error "XRT_LIB_DIR is undefined"
#endif

namespace xrt_core::detail::wsl2 {

// Abstraction for adapter opened by D3DKMTEnumAdapters3. Move semantics for
// storing in container, while making sure all enumerated adapters are
// eventually closed using D3DKMTCloseAdapter.
struct adapter
{
  D3DKMT_ADAPTERINFO m_info = {};

  adapter() = default;

  ~adapter()
  {
    if (!m_info.hAdapter)
      return;

    D3DKMT_CLOSEADAPTER close_adapter_args = {};
    close_adapter_args.hAdapter = m_info.hAdapter;
    D3DKMTCloseAdapter(&close_adapter_args);
  }

  // Movable
  adapter(adapter&& rhs) noexcept
    : m_info(std::move(rhs.m_info))
  {
    // Avoid double close
    rhs.m_info.hAdapter = 0;
  }

  // Not copyable
  adapter(const adapter&) = delete;
  adapter& operator=(const adapter&) = delete;

  D3DKMT_HANDLE
  handle() const
  {
    return m_info.hAdapter;
  }

  // Driver description string for this adapter (e.g. "AMD XDNA(TM) NPU").
  std::string
  driver_description() const
  {
    D3DKMT_QUERYADAPTERINFO query_adapter_info_args = {};
    D3DKMT_DRIVER_DESCRIPTION query_driver_description = {};
    query_adapter_info_args.hAdapter = handle();
    query_adapter_info_args.Type = KMTQAITYPE_DRIVER_DESCRIPTION;
    query_adapter_info_args.pPrivateDriverData = &query_driver_description;
    query_adapter_info_args.PrivateDriverDataSize = sizeof(query_driver_description);
    if (!NT_SUCCESS(D3DKMTQueryAdapterInfo(&query_adapter_info_args)))
      throw std::runtime_error("D3DKMTQueryAdapterInfo failed KMTQAITYPE_DRIVER_DESCRIPTION");

    // WCHAR[] -> narrow string (driver description is ASCII in practice).
    std::string desc;
    for (auto c : query_driver_description.DriverDescription) {
      if (c == 0)
        break;

      desc += static_cast<char>(c);
    }
    return desc;
  }
};

// Abstraction to manage list of enumerated adapters
struct adapter_list
{
  std::vector<adapter> m_adapters;

  adapter_list()
  {
    // Determine size of adapter list
    D3DKMT_ENUMADAPTERS3 enum_adapters_args = {};
    enum_adapters_args.Filter.IncludeComputeOnly = 1;
    auto status = D3DKMTEnumAdapters3(&enum_adapters_args);
    if (status != STATUS_SUCCESS)
      throw std::runtime_error("D3DKMTEnumAdapters3 failed");

    m_adapters.resize(enum_adapters_args.NumAdapters);

    // Enumerate adapters
    enum_adapters_args.pAdapters = reinterpret_cast<D3DKMT_ADAPTERINFO*>(m_adapters.data());
    status = D3DKMTEnumAdapters3(&enum_adapters_args);
    if (status != STATUS_SUCCESS)
      throw std::runtime_error("D3DKMTEnumAdapters3 failed");
    m_adapters.resize(enum_adapters_args.NumAdapters);
  }

  // Return first matching adapter
  const adapter*
  find(const std::string& match)
  {
    for (const auto& a : m_adapters) {
      if (a.driver_description() == match)
        return &a;
    }

    return nullptr;
  }
};

} // xrt_core::detail::wsl2

namespace xrt_core::detail {

namespace sfs = std::filesystem;

// Get XRT install path from DSO location or compile time constant.
//
// Mirrors detail/linux/xilinx_xrt.h's dladdr()-based approach verbatim —
// see that file for the full rationale (AIESW-32875 relocatability,
// SWSPLAT-24084/CWE-426 security note on dladdr() vs. env-var lookups).
// WSL has no "driver store" install-path concept to query instead, so
// there is no WSL-specific logic needed here.
sfs::path
xilinx_xrt()
{
  Dl_info info = {};
  if (::dladdr(reinterpret_cast<const void*>(&xilinx_xrt), &info) != 0
      && info.dli_fname && *info.dli_fname) {
    sfs::path xrt_root = sfs::path(info.dli_fname).parent_path();
    constexpr const char* xrt_lib_dir = XRT_LIB_DIR;
    for (const auto& comp : sfs::path(xrt_lib_dir)) {
      if (comp.empty() || comp == comp.root_directory())
        continue;
      xrt_root = xrt_root.parent_path();
    }
    if (!xrt_root.empty())
      return xrt_root;
  }

  // dladdr() failed: fall back to compile-time install prefix.
  return sfs::path(XRT_INSTALL_PREFIX);
}

// See detail/linux/xilinx_xrt.h for the full search-order documentation;
// identical here since WSL has nothing extra to add to it.
std::vector<sfs::path>
platform_repo_path()
{
  std::vector<sfs::path> paths;
  auto xrt = xrt_core::environment::xilinx_xrt();

  if (xrt.filename() == "xrt")
    paths.push_back(xrt / "share");
  else
    paths.push_back(xrt / "share/xrt");

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
