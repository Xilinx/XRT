/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#ifdef _WIN32
# pragma warning ( disable : 4996 4706 )
#endif

namespace hal = xrt_xocl::hal;
namespace hal2 = xrt_xocl::hal2;
namespace bfs = boost::filesystem;

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
directoryOrError(const bfs::path& path)
{
  if (!bfs::is_directory(path))
    throw std::runtime_error("No such directory '" + path.string() + "'");
}

static std::string&
propeFunc()
{
  static std::string sPropeFunc = "xclProbe";
  return sPropeFunc;
}

static std::string&
versionFunc()
{
  static std::string sVersionFunc = "xclVersion";
  return sVersionFunc;
}

static boost::filesystem::path&
dllExt()
{
#ifdef _WIN32
  static boost::filesystem::path sDllExt(".dll");
#else
  static boost::filesystem::path sDllExt(".so.2");
#endif
  return sDllExt;
}

inline bool
isDLL(const bfs::path& path)
{
  return (bfs::exists(path)
          && bfs::is_regular_file(path)
          && ends_with(path.string(), dllExt().string()));
}

boost::filesystem::path
dllpath(const boost::filesystem::path& root, const std::string& libnm)
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
  auto delHandle = [](void* handle) {
    xrt_core::dlclose(handle);
  };
  using handle_type = std::unique_ptr<void,decltype(delHandle)>;

  auto handle = handle_type(xrt_core::dlopen(dll.c_str(), RTLD_LAZY | RTLD_GLOBAL),delHandle);
  if (!handle)
    throw std::runtime_error("Failed to open HAL driver '" + dll + "'\n" + xrt_core::dlerror());

  typedef unsigned int (* probeFuncType)();

  auto probeFunc = (probeFuncType)xrt_core::dlsym(handle.get(), propeFunc().c_str());
  if (!probeFunc)
    return;

  unsigned pmdCount = 0;

  if (count || (count = probeFunc()) || pmdCount) {
  // Version of HAL
    typedef unsigned int (* versionFuncType)();
    auto vFunc = (versionFuncType)xrt_core::dlsym(handle.get(), versionFunc().c_str());
    auto version = 1;
    if (vFunc)
      version = vFunc();

    if (version==1) {
      throw std::runtime_error("Legacy HAL version " + std::to_string(version) + " not supported");
    }
    else if (version==2) {
      hal2::createDevices(devices,dll,handle.release(),count);
    }
    else
      throw std::runtime_error("HAL version " + std::to_string(version) + " not supported");
  }
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

  // xrt
  bfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));

#if defined (__aarch64__) || defined (__arm__)
  if (xrt.empty()) {
    xrt = bfs::path("/usr");
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

  return devices;
}


// Call to load_xdp comes from function_logger once per application run if 
//  profile is enabled.
void
load_xdp()
{
  struct xdp_once_loader
  {
    xdp_once_loader()
    {
      bfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));

      if (xrt.empty()) {
#if defined (__aarch64__) || defined (__arm__)
        xrt = bfs::path("/usr");
#else
        throw std::runtime_error("Library oclxdp not found! XILINX_XRT not set");
#endif
      }
      bfs::path xrtlib(xrt / "lib");
      directoryOrError(xrtlib);
      auto libname = dllpath(xrt, "oclxdp");
      if (!isDLL(libname)) {
        throw std::runtime_error("Library " + libname.string() + " not found!");
      }
      auto handle = xrt_core::dlopen(libname.string().c_str(), RTLD_NOW | RTLD_GLOBAL);
      if (!handle)
        throw std::runtime_error("Failed to open XDP library '" + libname.string() + "'\n" + xrt_core::dlerror());

      typedef void (* xdpInitType)();

      const std::string s = "initXDPLib";
      auto initFunc = (xdpInitType)xrt_core::dlsym(handle, s.c_str());
      if (!initFunc)
        throw std::runtime_error("Failed to initialize XDP library, '" + s +"' symbol not found.\n" + xrt_core::dlerror());

      initFunc();
    }
  };

  // 'magic static' is thread safe per C++11
  static xdp_once_loader xdp_loaded;
}

}} // hal,xcl
