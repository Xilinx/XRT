/**
 * Copyright (C) 2019 Xilinx, Inc
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
#define XRT_CORE_COMMON_SOURCE
#include "system.h"
#include "device.h"
#include "gen/version.h"

#include <vector>
#include <map>
#include <memory>
#include <mutex>

namespace {

static std::vector<std::weak_ptr<xrt_core::device>> mgmtpf_devices(16); // fix size
static std::vector<std::weak_ptr<xrt_core::device>> userpf_devices(16); // fix size
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
    throw std::runtime_error("singleton ctor error");
  singleton = this;
}

inline system&
instance()
{
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
    throw std::runtime_error("Could not open device");

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
  auto device = mgmtpf_devices[id].lock();
  if (!device) {
    device = instance().get_mgmtpf_device(id);
    mgmtpf_devices[id] = device;
  }
  return device;
}

std::pair<uint64_t, uint64_t>
get_total_devices(bool is_user)
{
  return instance().get_total_devices(is_user);
}

system::monitor_access_type
get_monitor_access_type()
{
  return instance().get_monitor_access_type();
}

} // xrt_core
