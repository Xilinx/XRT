/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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
#include "xrt/util/memory.h"

#include <dlfcn.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

namespace hal = xrt::hal;
namespace hal2 = xrt::hal2;
namespace bfs = boost::filesystem;
//namespace pmd = xrt::pmd;

namespace {

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

static const char*
getPlatform()
{
#if defined(__aarch64__)
  return "aarch64";
#elif defined(__arm__)
  return "arm64";
#elif defined(__x86_64__)
  return "x86_64";
#elif defined(__powerpc64__)
  return "ppc64le";
#else
# error("No driver directory for platform")
#endif
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

//#ifdef PMD_OCL
//static std::string&
//pmdPropeFunc()
//{
//  static std::string sPropeFunc = "pmdProbe";
//  return sPropeFunc;
//}
//#endif

static boost::filesystem::path&
dllExt()
{
  static boost::filesystem::path sDllExt(".so");
  return sDllExt;
}

inline bool
isDLL(const bfs::path& path)
{
  return (bfs::exists(path)
          && bfs::is_regular_file(path)
          && path.extension()==dllExt());
}

static bool
isEmulationMode()
{
  static bool val = (std::getenv("XCL_EMULATION_MODE") != nullptr);
  return val;
}

// Open the HAL implementation dll and construct a hal::device for
// each board detected by the implementation
static void
createHalDevices(hal::device_list& devices, const std::string& dll, unsigned int count=0)
{
  auto delHandle = [](void* handle){dlclose(handle);};
  typedef std::unique_ptr<void,decltype(delHandle)> handle_type;

  auto handle = handle_type(dlopen(dll.c_str(), RTLD_LAZY | RTLD_GLOBAL),delHandle);
  if (!handle)
    throw std::runtime_error("Failed to open HAL driver '" + dll + "'\n" + dlerror());

  typedef unsigned int (* probeFuncType)();

  auto probeFunc = (probeFuncType)dlsym(handle.get(), propeFunc().c_str());
  if (!probeFunc)
    return;

  unsigned pmdCount = 0;

  if (count || (count = probeFunc()) || pmdCount) {
  // Version of HAL
    typedef unsigned int (* versionFuncType)();
    auto vFunc = (versionFuncType)dlsym(handle.get(), versionFunc().c_str());
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

static void
loadHalDevices(hal::device_list& devices, const bfs::path& dir)
{
  // Iterator directory looking for driver dlls
  bfs::directory_iterator end;
  for (bfs::directory_iterator itr(dir);itr!=end;++itr) {
    bfs::path file(itr->path());
    if (isDLL(file))
    {
      if (!file.filename().compare("libxrt_hwemu.so") || !file.filename().compare("libxrt_swemu.so") || !file.filename().compare("libcommon_em.so"))
        continue;
      createHalDevices(devices,file.string());
    }
  }
}

} // namespace

namespace xrt { namespace hal {

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

  bfs::path opencl(emptyOrValue(getenv("XILINX_OPENCL")));
  if (!opencl.empty()) {
    directoryOrError(opencl);
    bfs::path path(opencl / "runtime/platforms");
    bfs::directory_iterator end;
    if(bfs::exists(path)) {
     for (bfs::directory_iterator itr(path); itr!=end; ++itr) {
       bfs::path p(itr->path());
       if (!p.filename().compare("hw_em") || !p.filename().compare("sw_em") || isEmulationMode())
         continue;
       p /= "driver";
       if (bfs::is_directory(p))
         loadHalDevices(devices,p);
     }
    }
  }

  // [optional]/xrt
  bfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));
  if (!xrt.empty() && !isEmulationMode()) {
    directoryOrError(xrt);
    bfs::path p(xrt / "lib");
    if (bfs::is_directory(p))
      loadHalDevices(devices,p);
  }

  bfs::path sdaccel(emptyOrValue(getenv("XILINX_SDX")));
  if (xrt.empty() && !sdaccel.empty() && !isEmulationMode()) {
    directoryOrError(sdaccel);
    bfs::path p(sdaccel / "data/sdaccel/pcie");
    p /= getPlatform();
    if (bfs::is_directory(p))
      loadHalDevices(devices,p);
  }

  if (!sdaccel.empty() && isEmulationMode()) {
    bfs::path em_platform = (strcmp(getPlatform(),"aarch64") == 0) ? "zynqu" : (((strcmp(getPlatform(),"arm64")==0) ? "zynq":"generic_pcie")) ;
    // Load emulation HAL
    bfs::path hw_em(sdaccel / "data/emulation/unified/hw_em" / em_platform / "driver/libhw_em.so");
    if(!xrt.empty()) {
      bfs::path hw_em_from_xrt (xrt / "lib/libxrt_hwemu.so");
      if (isDLL(hw_em_from_xrt)) {
        hw_em = hw_em_from_xrt;
      }
    }

    //give high priority to the driver provided in sdaccel.ini
    std::string hw_em_driver_path = xrt::config::get_hw_em_driver();
    if (!hw_em_driver_path.compare("null"))
      hw_em_driver_path.clear();

    if(hw_em_driver_path.size())
      hw_em = hw_em_driver_path;

    int numHwEm = 0;
    if (isDLL(hw_em)) {
      numHwEm = devices.size();
      createHalDevices(devices,hw_em.string());
      numHwEm = devices.size() - numHwEm;
    }

    bfs::path sw_em (sdaccel / "data/emulation/unified/cpu_em" / em_platform / "driver/libcpu_em.so");
    if(!xrt.empty()) {
      bfs::path sw_em_from_xrt (xrt / "lib/libxrt_swemu.so");
      if (isDLL(sw_em_from_xrt)) {
        sw_em = sw_em_from_xrt;
      }
    }

    //give high priority to the driver provided in sdaccel.ini
    std::string sw_em_driver_path = xrt::config::get_sw_em_driver();
    if (!sw_em_driver_path.compare("null"))
      sw_em_driver_path.clear();

    if(sw_em_driver_path.size())
      sw_em = sw_em_driver_path;

    if (isDLL(sw_em))
      // sw_emu uses the json file used to by hwem so sw-em will match hw-em
      createHalDevices(devices,sw_em.string());
  }

  if (xrt.empty() && sdaccel.empty() && opencl.empty())
    throw std::runtime_error("Either XILINX_OPENCL or XILINX_SDX must be set");

  return devices;
}


// Call to load_xdp comes from two places, but the dll should be loaded only once.
// It is called from function_logger once per application run if app_debug or profile is enabled.
// It is called from device once per xclbin load, if xclbin has debug_data in it.
void
load_xdp()
{
  struct xdp_once_loader
  {
    xdp_once_loader()
    {
      bfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));
      bfs::path libname ("libxdp.so");
      if (xrt.empty()) {
        throw std::runtime_error("Library " + libname.string() + " not found! XILINX_XRT not set");
      }
      bfs::path p(xrt / "lib");
      directoryOrError(p);
      p /= libname;
      if (!isDLL(p)) {
        throw std::runtime_error("Library " + p.string() + " not found!");
      }
      auto handle = dlopen(p.string().c_str(), RTLD_LAZY | RTLD_GLOBAL);
      if (!handle)
        throw std::runtime_error("Failed to open XDP library '" + p.string() + "'\n" + dlerror());

      typedef void (* xdpInitType)();

      const std::string s = "initXDPLib";
      auto initFunc = (xdpInitType)dlsym(handle, s.c_str());
      if (!initFunc)
        throw std::runtime_error("Failed to initialize XDP library, '" + s +"' symbol not found.\n" + dlerror());

      initFunc();
    }
  };

  // 'magic static' is thread safe per C++11
  static xdp_once_loader xdp_loaded;
}

}} // hal,xcl
