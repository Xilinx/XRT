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

#include "platform.h"
#include "device.h"
#include "debug.h"

#include "xocl/xclbin/xclbin.h"
#include "xrt/scheduler/scheduler.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <fstream>
#include <iostream>
#include <cassert>

#ifdef _WIN32
#pragma warning ( disable : 4996 )
#endif

namespace {

static xocl::platform* g_platform = nullptr;

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

} // namespace

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
          || driverLibraryName.find("swemu.so") != std::string::npos)
        m_swem.push_back(&dev);
      else if (driverLibraryName.find("hwemu.so") != std::string::npos)
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
  : m_device_mgr(std::make_unique<xrt_device_manager>())
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  assert(!g_platform);
  g_platform = this;

  XOCL_DEBUG(std::cout,"xocl::platform::platform(",m_uid,")\n");

  if (is_sw_emulation()) {
    while (auto swem_device = m_device_mgr->get_swem_device()) {
      auto udev = std::make_unique<xocl::device>(this,swem_device);
      auto dev = udev.release();
      add_device(dev);
      dev->release();
    }
  }
      
  if (is_hw_emulation()) {
    while (auto hwem_device = m_device_mgr->get_hwem_device()) {
      auto udev = std::make_unique<xocl::device>(this,hwem_device);
      auto dev = udev.release();
      add_device(dev);
      dev->release();
    }
  }

  //User can target either emulation or board. Not both at the same time.
  if (!is_emulation() && m_device_mgr->has_hw_devices()) {
    while (xrt::device* hw_device = m_device_mgr->get_hw_device()) {
      auto udev = std::make_unique<xocl::device>(this,hw_device);
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
}

platform::
~platform()
{
  XOCL_DEBUG(std::cout,"xocl::platform::~platform(",m_uid,")\n");
  try {
    xrt::scheduler::stop();
    g_platform = nullptr;
  }
  catch (const std::exception& ex) {
    XOCL_PRINTF("Unexpected exception in platform dtor '%s'\n",ex.what());
  }
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

} // xocl
