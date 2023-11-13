/**
 * Copyright (C) 2020 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

// Local - Include files
#include "system_linux.h"
#include "device_linux.h"
#include "core/common/time.h"

// 3rd Party Library - Include files
#include <boost/property_tree/ini_parser.hpp>
#include <boost/format.hpp>

// System - Include files
#include <fstream>
#include <memory>
#include <thread>
#include <unistd.h>

#if defined(__aarch64__) || defined(__arm__) || defined(__mips__)
  #define MACHINE_NODE_PATH "/proc/device-tree/model"
#elif defined(__PPC64__)
  #define MACHINE_NODE_PATH "/proc/device-tree/model-name"
  // /proc/device-tree/system-id may be 000000
  // /proc/device-tree/model may be 00000
#elif defined (__x86_64__)
  #define MACHINE_NODE_PATH "/sys/devices/virtual/dmi/id/product_name"
#else
#error "Unsupported platform"
  #define MACHINE_NODE_PATH ""
#endif

namespace {

// Singleton registers with base class xrt_core::system
// during static global initialization.  If statically
// linking with libxrt_core, then explicit initialiation
// is required
static xrt_core::system_linux*
singleton_instance()
{
  static xrt_core::system_linux singleton;
  return &singleton;
}

// Dynamic linking automatically constructs the singleton
struct X
{
  X() { singleton_instance(); }
} x;

static boost::property_tree::ptree
driver_version(const std::string& driver)
{
  boost::property_tree::ptree _pt;
  std::string ver("unknown");
  std::string hash("unknown");
  //dkms flow is not available for zocl
  //so version.h file is not available at zocl build time
#if defined(XRT_DRIVER_VERSION)
  std::string zocl_driver_ver = XRT_DRIVER_VERSION;
  std::stringstream ss(zocl_driver_ver);
  getline(ss, ver, ',');
  getline(ss, hash, ',');
#endif

  _pt.put("name", driver);
  _pt.put("version", ver);
  _pt.put("hash", hash);

  return _pt;
}

}

namespace xrt_core {

void
system_linux::
get_driver_info(boost::property_tree::ptree &pt)
{
  boost::property_tree::ptree _ptDriverInfo;
  _ptDriverInfo.push_back( std::make_pair("", driver_version("zocl") ));
  pt.put_child("drivers", _ptDriverInfo);
}

std::pair<device::id_type, device::id_type>
system_linux::
get_total_devices(bool is_user) const
{
  device::id_type num = xclProbe();
  return std::make_pair(num, num);
}

void
system_linux::
scan_devices(bool verbose, bool json) const
{
  std::cout << "TO-DO: scan_devices\n";
  verbose = verbose;
  json = json;
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::id_type id) const
{
  return xrt_core::get_userpf_device(xclOpen(id, nullptr, XCL_QUIET));
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::handle_type handle, device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<device_linux>(new device_linux(handle, id, true));
}

std::shared_ptr<device>
system_linux::
get_mgmtpf_device(device::id_type id) const
{
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<device_linux>(new device_linux(nullptr, id, false));
}

void
system_linux::
program_plp(const device* dev, const std::vector<char> &buffer) const
{
  throw std::runtime_error("plp program is not supported");
}

namespace edge_linux {

std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id)
{
  singleton_instance(); // force loading if necessary
  return xrt_core::get_userpf_device(device_handle, id);
}

} // edge_linux

} // xrt_core
