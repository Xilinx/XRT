// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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
   * get_device_id() - Convert string to device index
   *
   * Default implementation converts argument string to device index.
   *
   * Implement in specialized classes for conversion of special
   * device string formats, e.g. BDF.
   *
   * The native APIs have a xrt::device constructor that takes
   * a string.  The implementation of that constructor relies
   * on this function converting the string to a device index.
   */
  XRT_CORE_COMMON_EXPORT
  virtual device::id_type
  get_device_id(const std::string& str) const;

  /**
   */
  virtual std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
  get_bdf_info(device::id_type, bool) const
  {
    return std::make_tuple(uint16_t(0), uint16_t(0), uint16_t(0), uint16_t(0));
  }

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
  program_plp(const device*, const std::vector<char>&, bool) const
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
 * @brief Get the bdf info object
 * 
 * @param id 
 * @param is_user 
 * @return std::tuple<uint16_t, uint16_t, uint16_t, uint16_t> 
 */
XRT_CORE_COMMON_EXPORT
std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
get_bdf_info(device::id_type id, bool is_user = true);

/**
 * get_total_devices() - Get total devices and total usable devices
 *
 * Return: Pair of total devices and usable devices
 */
XRT_CORE_COMMON_EXPORT
std::pair<device::id_type, device::id_type>
get_total_devices(bool is_user);

/**
 * get_device_id() - Convert str to device index
 *
 * This function supports string formatted as BDF for systems where
 * BDF is used.  By default the function converts the argument string
 * to a number.
 *
 * The native APIs have a xrt::device constructor that takes a string
 * argument.  The implementation of that constructor relies on this
 * function converting a bdf to a device index.
 */
XRT_CORE_COMMON_EXPORT
device::id_type
get_device_id(const std::string& str);

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
program_plp(const device* dev, const std::vector<char> &buffer, bool force);
} //xrt_core

#endif /* CORE_SYSTEM_H */
