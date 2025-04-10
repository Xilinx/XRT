// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
#include "shim.h"
#include "system_linux.h"
#include "device_linux.h"
#include "core/common/message.h"
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
#endif

#ifdef EDGE_VE2_XDNA
#include "shim/xdna_device.h"
#endif

// This system class enumerates both Traditional Edge devices(Zocl driver)
// and Edge VE2 devices(AIARM driver).
// Map for holding valid devices of combined shim
// [int, device type] -> [device index, shim/driver type]
// As either of AIARM or ZOCL or both can be active together
// we need mapping of device id to device type
namespace {
enum class dev_type : uint16_t
{
  aiarm = 0,
  zocl = 1,
  aiarm_xdna = 2
};
static std::unordered_map<uint16_t, dev_type> dev_map;

namespace fs = std::filesystem;

static void
enumerate_accel_devices(unsigned int& device_count)
{
  // For Ve2 accel device (xdna driver) we search /sys/class/accel/accel* entries
  // to find the device node for our accel device
  // We match device tree entry 'telluride_drm'
  // Checking for AIARM accel device entry by getting accel name from
  // paths - /sys/class/accel/accel*/device/of_node/name
  const std::string of_node_name{"telluride_drm"};
  const std::string base_path = "/sys/class/accel";
  const std::string of_node_path = "/device/of_node/name";
  const std::regex accel_regex("accel.*");
  std::string accel_dev_name;

  try {
    if (!fs::exists(base_path)) {
      throw std::runtime_error("Device search path: " + base_path + " doesn't exist\n");
    }

    for (const auto& entry : fs::directory_iterator(base_path)) {
      if (fs::is_directory(entry) && std::regex_match(entry.path().filename().string(), accel_regex)) {
        const std::string accel_file_path = entry.path().string() + of_node_path;
        if (fs::exists(accel_file_path)) {
          std::ifstream accel_file(accel_file_path);
          std::string name;
          std::getline(accel_file, name);
	  // trim \0 at the end for proper comparision
	  if (!name.empty() && name.back() == '\0')
	    name = name.substr(0, name.size() - 1);

	  if (name.compare(of_node_name) == 0) {
            accel_dev_name = entry.path().filename().string();
	    break;
	  }
        }
      }
    }

    if (accel_dev_name.empty())
      throw std::runtime_error("Entry not found\n");

    // check accel file existance and insert device in map
    const std::string accel_dev_sym_dir{"/dev/accel/"};
    if (fs::exists(accel_dev_sym_dir + accel_dev_name))
      dev_map[device_count++] = dev_type::aiarm_xdna;
  }
  catch (const std::exception& e) {
    std::string msg = "AIARM accel device not found : " + std::string(e.what());
    xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", msg);
  }
}

static void
enumerate_render_devices(unsigned int& device_count)
{
  // For Render kind of devices a symlink entry is found in /dev/dri/by-path
  // with device tree node name of corresponding device
  // On Edge traditional devices 'zyxclmm_drm' is the name of zocl node in device tree
  // and on Ve2 render device 'telluride_drm' is the name in device tree
  // A symlink to render devices are created based on these node names
  auto match = [](const std::string& dir_path, const std::regex& filter) {
    if (!fs::exists(dir_path)) {
      throw std::runtime_error("Device search path: " + dir_path + " doesn't exist\n");
    }

    fs::directory_iterator end_itr;
    for (fs::directory_iterator itr{dir_path}; itr != end_itr; ++itr) {
      if (std::regex_match(itr->path().filename().string(), filter)) {
        return fs::read_symlink(itr->path()).filename().string();
      }
    }

    throw std::runtime_error("Device node symlink cannot be found\n");
  };

  // Checking for Render device entries (aiarm, zocl)
  static const std::regex aiarm_filter{"platform.*telluride_drm-render"};
  static const std::regex zocl_filter{"platform.*zyxclmm_drm-render"};

  // lambda function that checks validity of /dev/dri/renderD* device node
  // and inserts in global map with its type of device and increments device count
  auto validate_and_insert_in_map =
      [&](const std::regex& filter, const std::string& ver_name, const enum dev_type& type) {
    try {
      const std::string dev_path{"/dev/dri/"};
      const std::string render_dev_sym_dir{"/dev/dri/by-path/"};

      auto drm_dev_name = dev_path + match(render_dev_sym_dir, filter);
      if (!fs::exists(drm_dev_name))
        throw std::runtime_error(drm_dev_name + " device node doesn't exist");

      auto file_d = open(drm_dev_name.c_str(), O_RDWR);
      // lambda for closing fd
      auto fd_close = [](int* fd){
        if (fd && *fd >= 0) {
          close(*fd);
        }
      };
      auto fd = std::unique_ptr<int, decltype(fd_close)>(&file_d, fd_close);
      if (*fd < 0)
        throw std::runtime_error("Failed to open device file " + drm_dev_name);

      // validate DRM version name
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

      if (ioctl(*fd, DRM_IOCTL_VERSION, &version) != 0)
        throw std::runtime_error("Failed to get DRM version for device file " + drm_dev_name);

      if (std::strncmp(version.name, ver_name.c_str(), ver_name.length()) != 0)
        throw std::runtime_error("Driver DRM version check failed for device file " + drm_dev_name);

      dev_map[device_count++] = type;
    }
    catch (const std::exception& e) {
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", e.what());
    }
  };

  validate_and_insert_in_map(aiarm_filter, "AIARM", dev_type::aiarm);
  validate_and_insert_in_map(zocl_filter, "zocl", dev_type::zocl);
}

static unsigned int
enumerate_devices()
{
  /*
   * Zocl node is not present in some platforms static dtb, it gets loaded
   * using overlay dtb, drm device node is not created until zocl is present
   * So if enable_flat is set return 1 valid device
   * Valid for SOM kind of platforms
   */
  if (xrt_core::config::get_enable_flat())
    return 1;

  unsigned device_count = 0;
  enumerate_accel_devices(device_count);
  enumerate_render_devices(device_count);

  return device_count;
}

static xclDeviceHandle
get_device_handle(xrt_core::device::id_type id)
{
  try {
    auto total_devices = enumerate_devices();
    if (id >= total_devices) {
      xrt_core::message::send(xrt_core::message::severity_level::info, "XRT",
                              "Cannot find index " + std::to_string(id) + " \n");
      return static_cast<xclDeviceHandle>(nullptr);
    }

    auto type = dev_map[id];
#ifdef EDGE_VE2_XDNA
    if (type == dev_type::aiarm_xdna) {
      auto handle = new shim_xdna_edge::shim(id);
      return static_cast<xclDeviceHandle>(handle);
    }
#endif

#ifdef EDGE_VE2
    if (type == dev_type::aiarm) {
      auto handle = new aiarm::shim(id);
      if (!aiarm::shim::handleCheck(handle)) {
        delete handle;
        handle = XRT_NULL_HANDLE;
      }
      return static_cast<xclDeviceHandle>(handle);
    }
#endif

    if (type == dev_type::zocl) {
      auto handle = new ZYNQ::shim(id);
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
  device::id_type num = enumerate_devices();
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
  auto handle = get_device_handle(id);
  return xrt_core::get_userpf_device(handle);
}

std::shared_ptr<device>
system_linux::
get_userpf_device(device::handle_type handle, device::id_type id) const
{
  auto type = dev_map[id];

#ifdef EDGE_VE2_XDNA
  if (type == dev_type::aiarm_xdna) {
    // deliberately not using std::make_shared (used with weak_ptr)
    return std::shared_ptr<shim_xdna_edge::device_xdna>(new shim_xdna_edge::device_xdna(handle, id));
  }
#endif

#ifdef EDGE_VE2
  if (type == dev_type::aiarm) {
    // deliberately not using std::make_shared (used with weak_ptr)
    return std::shared_ptr<aiarm::device>(new aiarm::device(handle, id, true));
  }
#endif

  if (type == dev_type::zocl) {
    // deliberately not using std::make_shared (used with weak_ptr)
    return std::shared_ptr<device_linux>(new device_linux(handle, id, true));
  }

  throw std::runtime_error("get_userpf_device failed for device id : " + std::to_string(id));
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

} // edge_linux

} // xrt_core
