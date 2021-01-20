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

#ifndef XRT_CORE_SYSTEM_H
#define XRT_CORE_SYSTEM_H

#include "config.h"
#include "device.h"

#include <boost/property_tree/ptree.hpp>

namespace xrt_core {

/**
 * class system - Representation of host system
 *
 * The system class is a singleton base class specialized by different
 * types of systems we support, e.g. linux, windows, pcie, edge.
 *
 * The singleton handle is not available outside implementation so class
 * defintion is per construction for implementation use only.
 */
class system
{
protected:
  XRT_CORE_COMMON_EXPORT
  system();
public:
  // REMOVE
  virtual void
  get_xrt_info(boost::property_tree::ptree&) {}

  // REMOVE
  virtual void
  get_os_info(boost::property_tree::ptree&) {}

  // REMOVE
  virtual void
  get_devices(boost::property_tree::ptree&) const {}

  /**
   */
  virtual std::pair<device::id_type, device::id_type>
  get_total_devices(bool is_user = true) const = 0;

  /**
   * get_userpf_device() - Open a new device specified by index
   *
   * This function calls xclOpen to create a new shim handle from
   * which a core device is constructed.
   *
   * The returned device is managed, such that xclClose is called
   * when device is no longer referenced.
   */
  virtual std::shared_ptr<device>
  get_userpf_device(device::id_type id) const = 0;

  /**
   * get_userpf_device() - Get previous opened device from handle
   *
   * @hdl:  Handle for device
   * @id:   Device index
   *
   * The returned device is a pointer to the device opened previously
   * by a call to xclOpen.  This call could be explicit xclOpen in
   * host code, which must be followed by xclClose also in host code.
   *
   * The returned device is unmanaged, in other words xclClose is
   * not called when device goes out of scope.
   */
  virtual std::shared_ptr<device>
  get_userpf_device(device::handle_type hdl, device::id_type) const = 0;

  /**
   * get_mgmtpf_device() - construct mgmt device from device id
   */
  virtual std::shared_ptr<device>
  get_mgmtpf_device(device::id_type id) const = 0;

  /**
   * get_monitor_access_type() -
   *
   * Each system have different ways of accessing profiling
   * monitors (IPs in HW).  This function is used to determine
   * the access type.   It may be better if accessing the monitor
   * was part of the device class itself and thereby
   * transparent to end user, but for now the type is provided
   * here so that clients trigger off of the type.
   */
  enum class monitor_access_type { bar, mmap, ioctl };
  virtual monitor_access_type
  get_monitor_access_type() const
  {
    return monitor_access_type::bar;
  }

  virtual void
  program_plp(const device*, const std::vector<char>&) const
  {
    throw std::runtime_error("plp program is not supported");
  }
}; // system

/**
 */
XRT_CORE_COMMON_EXPORT
void
get_xrt_build_info(boost::property_tree::ptree& pt);

/**
 */
XRT_CORE_COMMON_EXPORT
void
get_xrt_info(boost::property_tree::ptree& pt);

/**
 */
XRT_CORE_COMMON_EXPORT
void
get_os_info(boost::property_tree::ptree& pt);

/**
 */
XRT_CORE_COMMON_EXPORT
void
get_devices(boost::property_tree::ptree& pt);

/**
 * get_total_devices() - Get total devices and total usable devices
 *
 * Return: Pair of total devices and usable devices
 */
XRT_CORE_COMMON_EXPORT
std::pair<uint64_t, uint64_t>
get_total_devices(bool is_user);

/**
 * get_userpf_device() - Open and create device specified by index
 *
 * This function calls xclOpen to create a new shim handle
 * The returned device is managed, such that xclClose is called
 * when device is deleted.
 */
XRT_CORE_COMMON_EXPORT
std::shared_ptr<device>
get_userpf_device(device::id_type id);

/**
 * get_userpf_device() - get userpf device from existing device handle
 *
 * @hdl:  Handle for device.  The handle is from xclOpen().
 *
 * This is a cached lookup to allow retrieving device associated
 * with device handle obtained from xclOpen().
 *
 * The returned device is unmanaged, meaning that the underlying
 * shim object is not closed when the device is deleted.
 */
XRT_CORE_COMMON_EXPORT
std::shared_ptr<device>
get_userpf_device(device::handle_type handle);

/**
 * get_userpf_device() - construct from existing handle and id
 *
 * @hdl:  Handle for device
 * @id:   Device index
 *
 * The returned device is unmanaged, meaning that the underlying
 * shim object is not closed when the device goes out of scope.
 *
 * This is used by shim level implementations to construct and
 * cache a device object as part of constructing shim level handle.
 * The function is called from shim constructors (xclOpen()).  After
 * registration, the xrt_core::device object can at all times be
 * retrived from just an hdl (xclDeviceHandle)
 */
XRT_CORE_COMMON_EXPORT
std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id);

/**
 * get_mgmtpf_device() - get mgmt device from device id
 *
 * This API is ambiguous in multi-threaded applications that
 * open a device in each thread. In these cases only the device
 * handle can be used to locate correspoding device object
 */
XRT_CORE_COMMON_EXPORT
std::shared_ptr<device>
get_mgmtpf_device(device::id_type id);

/**
 * get_monitor_access_type() - How should IPs be accessed from userspace
 *
 * Each system have different ways of accessing profiling
 * monitors (IPs in HW).  This function is used to determine
 * the access type.   It may be better if accessing the monitor
 * was part of the device class itself and thereby
 * transparent to end user, but for now the type is provided
 * here so that clients trigger off of the type.
 */
XRT_CORE_COMMON_EXPORT
system::monitor_access_type
get_monitor_access_type();

XRT_CORE_COMMON_EXPORT
void
program_plp(const device* dev, const std::vector<char> &buffer);

} //xrt_core

#endif /* CORE_SYSTEM_H */
