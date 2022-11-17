// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "system.h"
#include "device.h"
#include "module_loader.h"
#include "gen/version.h"


#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace {

static std::map<xrt_core::device::id_type, std::weak_ptr<xrt_core::device>> mgmtpf_device_map;
static std::map<xrt_core::device::handle_type, std::weak_ptr<xrt_core::device>> userpf_device_map;

// mutex to protect insertion
static std::mutex mutex;

}

namespace xrt_core {

// Singleton is initialized when libxrt_core is loaded
// A concrete system object is constructed during static
// global initialization.  Lifetime is until core library
// is unloaded.
system* singleton = nullptr;

system::
system()
{
  if (singleton)
    throw std::runtime_error
      ("Multiple instances of XRT core shim library detected, only one\n"
       "can be loaded at any given time.  Please check if application is\n"
       "explicity linked with XRT core library (xrt_core, xrt_hwemu, or\n"
       "xrt_swemu) and remove this linking. Use XCL_EMULATION_MODE set to\n"
       "either hw_emu or sw_emu if running in emulation mode.");

  singleton = this;
}

// get_device_id() -  Default conversion of device string
// Redefined in systems that support BDF.
device::id_type
system::
get_device_id(const std::string& str) const
{
  size_t pos = 0;
  auto id = std::stoul(str, &pos);
  if (pos != str.length())
    throw xrt_core::system_error(EINVAL, "Invalid device string '" + str + "'");
  return static_cast<device::id_type>(id);
}

static void
load_shim()
{
  // This is where the xrt_core library is loaded at run-time. Loading
  // of the library will create an instance of the system singleton
  // and set the singleton variable in this file. However, the
  // singleton, while set, can not be assumed to be valid until after
  // this function returns.  This is because the derived system class
  // could have constructor body that is executed after the base
  // class is constructed.
  static xrt_core::shim_loader shim;
}

inline system&
instance()
{
  // Multiple threads could enter here at the same time.  The first
  // thread will call the shim loader, where the singleton is set, but
  // not necessarily ready.  See comment in load_shim().
  static std::mutex mtx;
  std::lock_guard lk(mtx);

  if (!singleton)
    load_shim();

  if (singleton)
    return *singleton;

  throw std::runtime_error("system singleton is not loaded");
}

void
get_xrt_build_info(boost::property_tree::ptree& pt)
{
  pt.put("version",    xrt_build_version);
  pt.put("branch",     xrt_build_version_branch);
  pt.put("hash",       xrt_build_version_hash);
  pt.put("build_date", xrt_build_version_date);
}

void
get_xrt_info(boost::property_tree::ptree &pt)
{
  get_xrt_build_info(pt);
  instance().get_xrt_info(pt);
}

void
get_os_info(boost::property_tree::ptree& pt)
{
  instance().get_os_info(pt);
}

void
get_devices(boost::property_tree::ptree& pt)
{
  instance().get_devices(pt);
}

std::shared_ptr<device>
get_userpf_device(device::id_type id)
{
  // Construct device by calling xclOpen, the returned
  // device is cached and unmanaged
  auto device = instance().get_userpf_device(id);

  if (!device)
    throw std::runtime_error("Could not open device with index '"+ std::to_string(id) + "'");

  // Repackage raw ptr in new shared ptr with deleter that calls xclClose,
  // but leaves device object alone. The returned device is managed in that
  // it calls xclClose when going out of scope.
  auto close = [] (xrt_core::device* d) { d->close_device(); };
  return {device.get(), close};
}

std::shared_ptr<device>
get_userpf_device(device::handle_type handle)
{
  // Look up core device from low level shim handle
  // The handle is inserted into map as part of
  // calling xclOpen
  auto itr = userpf_device_map.find(handle);
  if (itr != userpf_device_map.end())
    return (*itr).second.lock();
  return nullptr;
}

std::shared_ptr<device>
get_userpf_device(device::handle_type handle, device::id_type id)
{
  // Check device map cache
  if (auto device = get_userpf_device(handle)) {
    if (device->get_device_id() != id)
        throw std::runtime_error("get_userpf_device: id mismatch");
    return device;
  }

  // Construct a new device object and insert in map.
  auto device = instance().get_userpf_device(handle,id);
  std::lock_guard<std::mutex> lk(mutex);
  userpf_device_map[handle] = device;  // create or replace
  return device;
}

std::shared_ptr<device>
get_mgmtpf_device(device::id_type id)
{
  // Check cache
  auto itr = mgmtpf_device_map.find(id);
  if (itr != mgmtpf_device_map.end())
    if (auto device = (*itr).second.lock())
      return device;

  // Construct a new device object and insert in map
  auto device = instance().get_mgmtpf_device(id);
  mgmtpf_device_map[id] = device;
  return device;
}

std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
get_bdf_info(device::id_type id, bool is_user)
{
  return instance().get_bdf_info(id, is_user);
}

std::pair<device::id_type, device::id_type>
get_total_devices(bool is_user)
{
  return instance().get_total_devices(is_user);
}

device::id_type
get_device_id(const std::string& str)
{
  return instance().get_device_id(str);
}

system::monitor_access_type
get_monitor_access_type()
{
  return instance().get_monitor_access_type();
}

void
program_plp(const device* dev, const std::vector<char> &buffer, bool force)
{
  instance().program_plp(dev, buffer, force);
}

} // xrt_core
