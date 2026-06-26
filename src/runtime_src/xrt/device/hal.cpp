// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2021 Xilinx, Inc
// Copyright (C) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.
#include "hal.h"

#include "core/common/dlfcn.h"
#include "core/common/device.h"
#include "core/common/module_loader.h"
#include "core/common/utils.h"

#include "xrt/experimental/xrt_system.h"

#include <filesystem>

namespace hal = xrt_xocl::hal;
namespace sfs = std::filesystem;
namespace hal2 = xrt_xocl::hal2;

namespace {

inline bool
ends_with(const std::string& str, const std::string& sub)
{
  auto p = str.rfind(sub);

  return (p==std::string::npos)
    ? false
    : (str.size() - p) == sub.size();
}

[[maybe_unused]]
static void
directoryOrError(const sfs::path& path)
{
  if (!sfs::is_directory(path))
    throw std::runtime_error("No such directory '" + path.string() + "'");
}

static std::filesystem::path&
dllExt()
{
#ifdef _WIN32
  static std::filesystem::path sDllExt(".dll");
#else
  static std::filesystem::path sDllExt(".so.2");
#endif
  return sDllExt;
}

inline bool
isDLL(const sfs::path& path)
{
  return (sfs::exists(path)
          && sfs::is_regular_file(path)
          && ends_with(path.string(), dllExt().string()));
}

std::filesystem::path
dllpath(const std::filesystem::path& root, const std::string& libnm)
{
#ifdef _WIN32
  return root / "bin" / (libnm + dllExt().string());
#else
  return root / XRT_LIB_DIR / ("lib" + libnm + dllExt().string());
#endif
}

[[maybe_unused]]
static bool
is_emulation()
{
  static bool val = !xrt_core::utils::getenv("XCL_EMULATION_MODE").empty();
  return val;
}

[[maybe_unused]]
static bool
is_sw_emulation()
{
  static auto xem = xrt_core::utils::getenv("XCL_EMULATION_MODE");
  static bool swem = xem.empty() ? false : xem.compare("sw_emu")==0;
  return swem;
}

[[maybe_unused]]
static bool
is_hw_emulation()
{
  static auto xem = xrt_core::utils::getenv("XCL_EMULATION_MODE");
  static bool hwem = xem.empty() ? false : xem.compare("hw_emu")==0;
  return hwem;
}

// Open the HAL implementation dll and construct a hal::device for
// each board detected by the implementation
static void
createHalDevices(hal::device_list& devices, const std::string& dll, unsigned int count=0)
{
  if (!count)
    count = xrt::system::enumerate_devices();

  if (count)
    hal2::createDevices(devices, dll, count);
}

} // namespace

namespace xrt_xocl { namespace hal {

device::
device()
{
}

device::
~device()
{
}

hal::device_list
loadDevices()
{
  hal::device_list devices;
#ifndef XRT_STATIC_BUILD
  // xrt
  auto xrt = xrt_core::environment::xilinx_xrt();

#if defined (__aarch64__) || defined (__arm__)
  if (xrt.empty()) {
    xrt = sfs::path("/usr");
  }
#endif

  if (!xrt.empty() && !is_emulation()) {
    directoryOrError(xrt);
    auto p = dllpath(xrt,"xrt_core");
    if (isDLL(p))
      createHalDevices(devices,p.string());
  }

  if (!xrt.empty() && is_hw_emulation()) {
    directoryOrError(xrt);

    auto hw_em_driver_path = xrt_xocl::config::get_hw_em_driver();
    if (hw_em_driver_path == "null") {
      auto p = dllpath(xrt,"xrt_hwemu");
      if (isDLL(p))
        hw_em_driver_path = p.string();
    }

    if (isDLL(hw_em_driver_path))
      createHalDevices(devices,hw_em_driver_path);
  }

  if (!xrt.empty() && is_sw_emulation()) {
    directoryOrError(xrt);

    auto sw_em_driver_path = xrt_xocl::config::get_sw_em_driver();
    if (sw_em_driver_path == "null") {
      auto p = dllpath(xrt,"xrt_swemu");
      if (isDLL(p))
        sw_em_driver_path = p.string();
    }

    if (isDLL(sw_em_driver_path))
      createHalDevices(devices,sw_em_driver_path);
  }

  if (xrt.empty())
    throw std::runtime_error("XILINX_XRT must be set");
#else
  createHalDevices(devices, "shim");
#endif

  return devices;
}

}} // hal,xcl
