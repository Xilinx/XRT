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
  get_xrt_info(boost::property_tree::ptree &pt) = 0;

  // REMOVE
  virtual void
  get_os_info(boost::property_tree::ptree &pt) = 0;

  // REMOVE
  virtual void
  get_devices(boost::property_tree::ptree &pt) const = 0;

  /**
   */
  virtual std::pair<device::id_type, device::id_type>
  get_total_devices(bool is_user = true) const = 0;

  /**
   * get_userpf_device() - construct from device id
   */
  virtual std::shared_ptr<device>
  get_userpf_device(device::id_type id) const = 0;

  /**
   * get_userpf_device() - construct from existing handle and id
   *
   * @hdl:  Handle for device
   * @id:   Device index
   */
  virtual std::shared_ptr<device>
  get_userpf_device(device::handle_type hdl, device::id_type) const = 0;

  /**
   * get_mgmtpf_device() - construct mgmt device from device id
   */
  virtual std::shared_ptr<device>
  get_mgmtpf_device(device::id_type id) const = 0;

}; // system

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
 * get_userpf_device() - construct from device id
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
 * with device handle obtained from xclOpen()
 */
XRT_CORE_COMMON_EXPORT
std::shared_ptr<device>
get_userpf_device(device::handle_type handle);

/**
 * get_userpf_device() - get userpf mdevice from existing handle and id
 *
 * @hdl:  Handle for device
 * @id:   Device index
 *
 * This is used by shim level implementations to construct and
 * cache a device object as part of constructing shim level handle.
 * The function is called from shim constructors (xclOpen()).  After
 * registration the xrt_core::device object can at all times be 
 * retrived from just an hdl (xclDeviceHandle)
 */
XRT_CORE_COMMON_EXPORT
std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id);

/**
 * get_mgmtpf_device() - get mgmt device from device id
 */
XRT_CORE_COMMON_EXPORT
std::shared_ptr<device>
get_mgmtpf_device(device::id_type id);

} //xrt_core

#endif /* CORE_SYSTEM_H */
