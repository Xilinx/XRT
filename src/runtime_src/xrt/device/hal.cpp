/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "hal.h"

#include "core/common/dlfcn.h"
#include "core/common/device.h"
#include "core/include/xrt/experimental/xrt_system.h"

#include <filesystem>

#ifdef _WIN32
# pragma warning ( disable : 4996 4706 4505 )
#endif

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

static const char*
emptyOrValue(const char* cstr)
{
  return cstr ? cstr : "";
}

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
  return root / "lib" / ("lib" + libnm + dllExt().string());
#endif
}

static bool
is_emulation()
{
  static bool val = (std::getenv("XCL_EMULATION_MODE") != nullptr);
  return val;
}

static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? (std::strcmp(xem,"sw_emu")==0) : false;
  return swem;
}

static bool
is_hw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool hwem = xem ? (std::strcmp(xem,"hw_emu")==0) : false;
  return hwem;
}

static bool
is_noop_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool noop = xem ? (std::strcmp(xem,"noop")==0) : false;
  return noop;
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
  sfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));

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

  if (!xrt.empty() && is_noop_emulation()) {
    directoryOrError(xrt);
    auto p = dllpath(xrt,"xrt_noop");
    if (isDLL(p))
      createHalDevices(devices,p.string());
  }

  if (xrt.empty())
    throw std::runtime_error("XILINX_XRT must be set");
#else
  createHalDevices(devices, "shim");
#endif

  return devices;
}

}} // hal,xcl
