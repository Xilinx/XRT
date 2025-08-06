// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
#include "shim.h"
#include "system_linux.h"
#include "device_linux.h"
#include "core/common/message.h"
#include "core/common/time.h"
#include "plugin/xdp/hal_profile.h"
#include "core/common/module_loader.h"
#include "dev.h"
#include "drv_zocl.h"

// 3rd Party Library - Include files
#include <boost/property_tree/ini_parser.hpp>
#include <boost/format.hpp>

// System - Include files
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <iostream>
#include <mutex>

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

// This system class enumerates both Traditional Edge devices(Zocl driver)
// and Edge VE2 devices(AIARM driver).
// Map for holding valid devices of combined shim
// [int, device type] -> [device index, shim/driver type]
// As either of AIARM or ZOCL or both can be active together
// we need mapping of device id to device type
namespace {

namespace driver_list {

static std::vector<std::shared_ptr<xrt_core::edge::drv>> drv_list; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static std::mutex mutex_driver_list; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void
append(std::shared_ptr<xrt_core::edge::drv> driver)
{
  std::lock_guard<std::mutex> lock(mutex_driver_list);
  drv_list.push_back(std::move(driver));
}

const std::vector<std::shared_ptr<xrt_core::edge::drv>>&
get()
{
  std::lock_guard<std::mutex> lock(mutex_driver_list);
  return drv_list;
}

} //namespace driver_list

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
  std::string path("/sys/module/");
  path += driver;
  path += "/version";
  //dkms flow is not available for zocl
  //so version.h file is not available at zocl build time
#if defined(XRT_DRIVER_VERSION)
  std::string zocl_driver_ver = XRT_DRIVER_VERSION;
  std::stringstream ss(zocl_driver_ver);
  getline(ss, ver, ',');
#endif
  std::ifstream stream(path);
  if (stream.is_open())
     getline(stream, hash);
  
  _pt.put("name", driver);
  _pt.put("version", ver);
  _pt.put("hash", hash);

  return _pt;
}

}

namespace xrt_core {

system_linux::
system_linux()
{
  // Load driver plug-ins. Driver list will be updated during loading.
  // Don't need to die on a plug-in loading failure.
  try {
    xrt_core::driver_loader plugins;
  }
  catch (const std::runtime_error& err) {
    xrt_core::send_exception_message(err.what(), "WARNING");
  }

  //zocl driver will be added last
  driver_list::append(std::make_shared<xrt_core::edge::drv_zocl>());

  for (const auto& driver : driver_list::get()) {
    driver->scan_devices(dev_list);
  }
}

std::shared_ptr<edge::dev>
system_linux::
get_edge_dev(unsigned index) const
{
  if (index < dev_list.size())
    return dev_list[index];

  // given index is not present in list
  throw std::runtime_error(" No such device with index '"+ std::to_string(index) + "'");
}

void
system_linux::
get_driver_info(boost::property_tree::ptree &pt)
{
  boost::property_tree::ptree _ptDriverInfo;

  for (const auto& drv : driver_list::get()) {
    boost::property_tree::ptree _drv = driver_version(drv->name());
    if (!_drv.empty())
      _ptDriverInfo.push_back( {"", _drv} );
  }
  pt.put_child("drivers", _ptDriverInfo);
}

std::pair<device::id_type, device::id_type>
system_linux::
get_total_devices(bool is_user) const
{
  device::id_type num = dev_list.size();
  return std::make_pair(num, num);
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
system_linux::
get_bdf_info(device::id_type id, bool is_user) const
{
    if (id < dev_list.size()) {
      auto device = get_userpf_device(id);
      return std::make_tuple(0, 0, 0, device->get_device_id());
    }

    throw std::runtime_error(" No such device with index '"+ std::to_string(id) + "'");
}

void
system_linux::
scan_devices(bool /*verbose*/, bool /*json*/) const
{
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::id_type id) const
{
  auto edge_dev = get_edge_dev(id);
  return xrt_core::get_userpf_device(edge_dev->create_shim(id));
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::handle_type handle, device::id_type id) const
{
  auto edge_dev = get_edge_dev(id);
  return edge_dev->create_device(handle, id);
}

std::shared_ptr<device>
system_linux::
get_mgmtpf_device(device::id_type id) const
{
  throw std::runtime_error("Not Supported\n");
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

void
register_driver(std::shared_ptr<xrt_core::edge::drv> driver)
{
  driver_list::append(std::move(driver));
}

std::shared_ptr<edge::dev>
get_dev(unsigned index)
{
  return singleton_instance()->get_edge_dev(index);
}

} // edge_linux

} // xrt_core
