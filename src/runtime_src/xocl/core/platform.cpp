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

#include "platform.h"
#include "device.h"
#include "debug.h"

#include "xocl/xclbin/xclbin.h"
#include "xrt/util/memory.h"
#include "xrt/scheduler/scheduler.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <fstream>
#include <iostream>
#include <cassert>

namespace {

static xocl::platform* g_platform = nullptr;

// conformance kernel hash -> xclbin file name
static std::map<std::string, std::string> global_conformance_xclbin_map;

static const char*
value_or_empty(const char* value)
{
  return value ? value : "";
}

static std::string
get_env(const char* env)
{
  return value_or_empty(std::getenv(env));
}

XOCL_UNUSED
static bool
is_emulation_mode()
{
  static bool emulation_mode = false;
  static bool initialized = false;
  if (!initialized) {
    std::string env = get_env("XCL_EMULATION_MODE");
    if(!env.empty() && (env=="sw_emu" || env=="hw_emu") )
      emulation_mode = true;
    initialized = true;
  } 
  return emulation_mode;
}

static std::vector<char>
read_file(const std::string& filename)
{
  std::ifstream istr(filename,std::ios::binary|std::ios::ate);
  if (!istr)
    throw xocl::error(CL_BUILD_PROGRAM_FAILURE,"Cannot not open '" + filename + "' for reading");

  auto pos = istr.tellg();
  istr.seekg(0,std::ios::beg);

  std::vector<char> buffer(pos);
  istr.read (&buffer[0],pos);

  return buffer;
}

static void
init_conformance()
{
  if (!std::getenv("XCL_CONFORMANCE"))
    return ;

  // iterate each xclbin in current directory
  namespace bfs = boost::filesystem;
  bfs::directory_iterator end;
  for (bfs::directory_iterator itr(".");itr!=end;++itr) {
    bfs::path file(itr->path());

    if (bfs::exists(file) && bfs::is_regular_file(file) && file.extension()==".xclbin") {
      auto xclbin = xocl::xclbin(read_file(file.string()));
      for (auto hash : xclbin.conformance_kernel_hashes())  {
        XOCL_DEBUG(std::cout,"(hash,file)=(",hash,",",file.string(),")\n");
        global_conformance_xclbin_map.emplace(hash,file.string());
      }
    }
  }
}

}

namespace xocl {

class platform::xrt_device_manager
{
  std::vector<xrt::device>  m_all; // owner
  std::vector<xrt::device*> m_hw;
  std::vector<xrt::device*> m_hwem;
  std::vector<xrt::device*> m_swem;

public:
  xrt_device_manager()
    : m_all(xrt::loadDevices())
  {
    if (m_all.empty())
      throw xocl::error(CL_DEVICE_NOT_FOUND,"No devices found");
    for (auto& dev : m_all) {
      std::string driverLibraryName = dev.getDriverLibraryName();
      if (driverLibraryName.find("sw_em.so") != std::string::npos
          || driverLibraryName.find("cpu_em.so") != std::string::npos)
        m_swem.push_back(&dev);
      else if (driverLibraryName.find("hw_em.so") != std::string::npos)
        m_hwem.push_back(&dev);
      else
        m_hw.push_back(&dev);
    }

    // The devices are partitioned into vectors that are popped off
    // the back.  This gives reverse order compared to how hw and
    // hw_em devices were processed in the past.  There is something
    // horribly wrong with these order assumptions.  Anyway, reverse
    // hw and hw_em vectors here.
    std::reverse(m_hw.begin(),m_hw.end());
    std::reverse(m_hwem.begin(),m_hwem.end());
    std::reverse(m_swem.begin(),m_swem.end());

    // Sanity checks
    if (m_hwem.size() != m_swem.size())
      throw xocl::error(CL_DEVICE_NOT_FOUND,"Emulation device mismatch");
  }

  bool
  has_hw_devices() const
  {
    return !m_hw.empty();
  }

  bool
  has_swem_devices() const
  {
    return !m_swem.empty();
  }

  bool
  has_hwem_devices() const
  {
    return !m_hwem.empty();
  }

  xrt::device*
  get_swem_device()
  {
    xrt::device* dev = nullptr;
    if (has_swem_devices()) {
      dev = m_swem.back();
      m_swem.pop_back();
    }
    return dev;
  }

  xrt::device*
  get_hwem_device()
  {
    xrt::device* dev = nullptr;
    if (has_hwem_devices()) {
      dev = m_hwem.back();
      m_hwem.pop_back();
    }
    return dev;
  }

  xrt::device*
  get_hw_device()
  {
    xrt::device* dev = nullptr;
    if (has_hw_devices()) {
      dev = m_hw.back();
      m_hw.pop_back();
    }
    return dev;
  }

  xrt::device*
  get_hwem_device(const std::string& name)
  {
    auto itr = std::find_if(m_hwem.begin(),m_hwem.end(),
                            [&name](const xrt::device* dev) {
                              return dev->getName()==name;
                            });
    if (itr==m_hwem.end())
      return nullptr;
    xrt::device* dev = *itr;
    m_hwem.erase(itr);
    return dev;
  }
};

platform::
platform()
  : m_device_mgr(xrt::make_unique<xrt_device_manager>())
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  assert(!g_platform);
  g_platform = this;

  XOCL_DEBUG(std::cout,"xocl::platform::platform(",m_uid,")\n");

  if (is_emulation_mode()) {
    while (auto hwem_device = m_device_mgr->get_hwem_device()) {
      auto swem_device = m_device_mgr->get_swem_device();
      auto udev = xrt::make_unique<xocl::device>(this,swem_device,hwem_device);
#ifndef PMD_OCL
      auto dev = udev.release();
      add_device(dev);
      dev->release();
#endif
    }
  }
    
  //User can target either emulation or board. Not both at the same time.
  if (!is_emulation_mode() && m_device_mgr->has_hw_devices()) {
    while (xrt::device* hw_device = m_device_mgr->get_hw_device()) {
      auto udev = xrt::make_unique<xocl::device>(this,hw_device,nullptr,nullptr);
      auto dev = udev.release();
      add_device(dev);
      dev->release();
    }
  }

  try {
    xrt::scheduler::start();
  }
  catch(const std::exception&) {
    throw error(CL_OUT_OF_HOST_MEMORY,"failed to allocate platform event_scheduler");
  }

  init_conformance();
}

platform::
~platform()
{
  xrt::scheduler::stop();
  g_platform = nullptr;
  XOCL_DEBUG(std::cout,"xocl::platform::~platform(",m_uid,")\n");
}

void
platform::
add_device(device* dev)
{
  m_devices.push_back(dev);
}

std::shared_ptr<platform>
platform::
get_shared_platform()
{
// The global platform is constructed when first accessed and deleted
// at program exit when the last reference goes away.  Shared
// ownership can be obtained through xocl::get_shared_platform();
  static auto global_platform = std::make_shared<platform>();
  return global_platform;
}

platform*
get_global_platform()
{
  static auto platform = platform::get_shared_platform().get();
  return platform;
}

std::shared_ptr<platform>
get_shared_platform()
{
  return platform::get_shared_platform();
}


std::vector<platform*>
get_platforms()
{
  std::vector<platform*> platforms;
  platforms.push_back(get_global_platform());
  return platforms;
}

unsigned int
get_num_platforms()
{
  return g_platform ? 1 : 0;
}

std::string
get_xilinx_opencl()
{
  static std::string xilinx_opencl = get_env("XILINX_OPENCL");
  return xilinx_opencl;
}

std::string
get_xilinx_sdx()
{
  static std::string xilinx_sdx = get_env("XILINX_SDX");
  return xilinx_sdx;
}

std::string
conformance_get_xclbin(const std::string& hash)
{
  auto itr = global_conformance_xclbin_map.find(hash);
  return itr==global_conformance_xclbin_map.end()
    ? ""
    : (*itr).second;
}

} // xocl


