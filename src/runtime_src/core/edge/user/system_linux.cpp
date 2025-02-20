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
#include "shim.h"
#include "system_linux.h"
#include "device_linux.h"
#include "core/common/time.h"

#include "plugin/xdp/hal_profile.h"

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

#ifdef EDGE_VE2
#include "shim/device.h"

// When EDGE_VE2 flag is set both zocl and VE2 shims are
// combined.
// Map for holding valid devices/shims of combined shim
// [int, device type] -> [device index, shim/driver type]
// As either of AIARM or ZOCL or both can be active together
// we need mapping of device id to device type
namespace {
enum class dev_type : uint16_t
{
  aiarm = 0,
  zocl = 1
};
static std::unordered_map<uint16_t, dev_type> dev_map;
}
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
scan_devices(bool /*verbose*/, bool /*json*/) const
{
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
#ifdef EDGE_VE2
  auto type = dev_map[id];
  if (type == dev_type::aiarm)
    return std::shared_ptr<aiarm::device>(new aiarm::device(handle, id, true));
  else if (type == dev_type::zocl)
    return std::shared_ptr<device_linux>(new device_linux(handle, id, true));

  throw std::runtime_error("get_userpf_device failed for device id : " + std::to_string(id));
#else
  // deliberately not using std::make_shared (used with weak_ptr)
  return std::shared_ptr<device_linux>(new device_linux(handle, id, true));
#endif
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

// Implementation of xclProbe and xclOpen used by system class.
// By default xclProbe detects only zocl device but when flag
// EDGE_VE2 is set shims of both traditional zocl and VE2
// are combined and xclProbe should detect both kinds of devices
#ifndef EDGE_VE2
unsigned
xclProbe()
{
  return xdp::hal::profiling_wrapper("xclProbe", [] {

  const std::string zocl_drm_device = "/dev/dri/" + get_render_devname();
  int fd;
  if (std::filesystem::exists(zocl_drm_device)) {
    fd = open(zocl_drm_device.c_str(), O_RDWR);
    if (fd < 0)
      return 0;
  }
  /*
   * Zocl node is not present in some platforms static dtb, it gets loaded
   * using overlay dtb, drm device node is not created until zocl is present
   * So if enable_flat is set return 1 valid device
   */
  else if (xrt_core::config::get_enable_flat())
    return 1;

  std::vector<char> name(128,0);
  std::vector<char> desc(512,0);
  std::vector<char> date(128,0);
  drm_version version;
  std::memset(&version, 0, sizeof(version));
  version.name = name.data();
  version.name_len = 128;
  version.desc = desc.data();
  version.desc_len = 512;
  version.date = date.data();
  version.date_len = 128;

  int result = ioctl(fd, DRM_IOCTL_VERSION, &version);
  if (result) {
    close(fd);
    return 0;
  }

  result = std::strncmp(version.name, "zocl", 4);
  close(fd);
  return (result == 0) ? 1 : 0;
  });
}

xclDeviceHandle
xclOpen(unsigned deviceIndex, const char*, xclVerbosityLevel)
{
  return xdp::hal::profiling_wrapper("xclOpen",
  [deviceIndex] {

  try {
    //std::cout << "xclOpen called" << std::endl;
    if (deviceIndex >= xclProbe()) {
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                       std::string("Cannot find index " + std::to_string(deviceIndex) + " \n"));
      return static_cast<xclDeviceHandle>(nullptr);
    }

    auto handle = new ZYNQ::shim(deviceIndex);
    bool checkDrmFD = xrt_core::config::get_enable_flat() ? false : true;
    if (!ZYNQ::shim::handleCheck(handle, checkDrmFD)) {
      delete handle;
      handle = XRT_NULL_HANDLE;
    }
    return static_cast<xclDeviceHandle>(handle);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }

  return static_cast<xclDeviceHandle>(XRT_NULL_HANDLE);

  }) ;
}
#else
// xclProbe and xclOpen definitions for combined shim
unsigned
xclProbe()
{
  static const std::string render_dev_sym_dir{"/dev/dri/by-path/"};
  std::string aiarm_render_devname;
  std::string zocl_render_devname;

  // On Edge AIE platforms 'telluride_drm' is the name of aiarm node in device tree
  // On Edge platforms 'zyxclmm_drm' is the name of zocl node in device tree
  // A symlink to render devices are created based on these node names
  try {
    static const std::regex aiarm_filter{"platform.*telluride_drm-render"};
    static const std::regex zocl_filter{"platform.*zyxclmm_drm-render"};
    if (!std::filesystem::exists(render_dev_sym_dir)) {
      std::string msg{"DRM device path - " + render_dev_sym_dir +
                      "doesn't exist, cannot detect devices"};
      xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", msg);
      return 0;
    }
    std::filesystem::directory_iterator end_itr;
    for (std::filesystem::directory_iterator itr{render_dev_sym_dir}; itr != end_itr; ++itr) {
      if (std::regex_match(itr->path().filename().string(), aiarm_filter)) {
        aiarm_render_devname = std::filesystem::read_symlink(itr->path()).filename().string();
      }
      if (std::regex_match(itr->path().filename().string(), zocl_filter)) {
        zocl_render_devname = std::filesystem::read_symlink(itr->path()).filename().string();
      }
    }
  }
  catch (const std::exception& e) {
    xrt_core::message::send(xrt_core::message::severity_level::error, "XRT",
                            std::string{"Unable to read renderD* path of devices"} + e.what());
    return 0;
  }

  auto check_validity = [](const std::string& drm_dev_name, const std::string& ver_name) {
    if (!std::filesystem::exists(drm_dev_name))
      return false;

    int fd = open(drm_dev_name.c_str(), O_RDWR);
    if (fd < 0)
      return false;

    std::vector<char> name(128,0);
    std::vector<char> desc(512,0);
    std::vector<char> date(128,0);
    drm_version version;
    std::memset(&version, 0, sizeof(version));
    version.name = name.data();
    version.name_len = 128;
    version.desc = desc.data();
    version.desc_len = 512;
    version.date = date.data();
    version.date_len = 128;
    int result = ioctl(fd, DRM_IOCTL_VERSION, &version);
    if (result) {
      close(fd);
      return false;
    }
    result = std::strncmp(version.name, ver_name.c_str(), ver_name.length());
    close(fd);
    return (result == 0) ? true : false;
  };

  unsigned device_count = 0;
  if (check_validity("/dev/dri/" + aiarm_render_devname, "AIARM")) {
    dev_map[device_count++] = dev_type::aiarm;
  }

  if (check_validity("/dev/dri/" + zocl_render_devname, "zocl")) {
    dev_map[device_count++] = dev_type::zocl;
  }

  return device_count;
}

xclDeviceHandle
xclOpen(unsigned deviceIndex, const char*, xclVerbosityLevel)
{
  try {
    if (deviceIndex >= xclProbe()) {
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                              "Cannot find index " + std::to_string(deviceIndex) + " \n");
      return static_cast<xclDeviceHandle>(nullptr);
    }

    auto type = dev_map[deviceIndex];

    if (type == dev_type::aiarm) {
      auto handle = new aiarm::shim(deviceIndex);
      if (!aiarm::shim::handleCheck(handle)) {
        delete handle;
        handle = XRT_NULL_HANDLE;
      }
      return static_cast<xclDeviceHandle>(handle);
    }
    else if (type == dev_type::zocl) {
      auto handle = new ZYNQ::shim(deviceIndex);
      if (!ZYNQ::shim::handleCheck(handle)) {
        delete handle;
        handle = XRT_NULL_HANDLE;
      }
      return static_cast<xclDeviceHandle>(handle);
    }
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return static_cast<xclDeviceHandle>(XRT_NULL_HANDLE);
}
#endif
