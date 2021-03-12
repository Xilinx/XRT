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

#include "platform.h"
#include "device.h"
#include "debug.h"

#include "xocl/xclbin/xclbin.h"

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

} // namespace

namespace xocl {

class platform::xrt_device_manager
{
  std::vector<xrt_xocl::device>  m_all; // owner
  std::vector<xrt_xocl::device*> m_dev; // handout

public:
  xrt_device_manager()
    : m_all(xrt_xocl::loadDevices())
  {
    if (m_all.empty())
      throw xocl::error(CL_DEVICE_NOT_FOUND,"No devices found");
    for (auto& dev : m_all)
      m_dev.push_back(&dev);

    // The devices are partitioned into vectors that are popped off
    // the back.  This gives reverse order compared to how hw and
    // hw_em devices were processed in the past.  There is something
    // horribly wrong with these order assumptions.  Anyway, reverse
    // hw and hw_em vectors here.
    std::reverse(m_dev.begin(),m_dev.end());
  }

  bool
  has_devices() const
  {
    return !m_dev.empty();
  }

  xrt_xocl::device*
  get_device()
  {
    xrt_xocl::device* dev = nullptr;
    if (has_devices()) {
      dev = m_dev.back();
      m_dev.pop_back();
    }
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

  if (m_device_mgr->has_devices()) {
    while (xrt_xocl::device* device = m_device_mgr->get_device()) {
      auto udev = std::make_unique<xocl::device>(this, device);
      auto dev = udev.release();
      add_device(dev);
      dev->release();
    }
  }
}

platform::
~platform()
{
  XOCL_DEBUG(std::cout,"xocl::platform::~platform(",m_uid,")\n");
  try {
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
