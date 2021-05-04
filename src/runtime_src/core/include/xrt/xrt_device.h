/*
 * Copyright (C) 2020-2021, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#ifndef _XRT_DEVICE_H_
#define _XRT_DEVICE_H_

#include "xrt.h"
#include "xrt/xrt_uuid.h"
#include "experimental/xrt_xclbin.h"

#ifdef __cplusplus
# include "xrt/detail/param_traits.h"
# include <memory>
# include <boost/any.hpp> // std::any c++17
#endif

/**
 * typedef xrtDeviceHandle - opaque device handle
 */
typedef void* xrtDeviceHandle;
  
#ifdef __cplusplus

namespace xrt_core {
class device;
}

namespace xrt {

namespace info {

/**    
 * @enum info::device - device information parameters.
 *
 *
 * Use with \ref xrt::device::get_info() to retrieve properties of the
 * device.  The type of the device properties is compile time defined
 * with param traits.
 */
enum class device : unsigned int {
  bdf,
  interface_uuid,
  kdma,
  max_clock_frequency_mhz,
  m2m,
  name,
  nodma,
  offline,
};

/**
 * Return type for xrt::device::get_info()
 */
XRT_INFO_PARAM_TRAITS(device::bdf, std::string);
XRT_INFO_PARAM_TRAITS(device::interface_uuid, xrt::uuid);
XRT_INFO_PARAM_TRAITS(device::kdma, std::uint32_t);
XRT_INFO_PARAM_TRAITS(device::max_clock_frequency_mhz, unsigned long);
XRT_INFO_PARAM_TRAITS(device::m2m, bool);
XRT_INFO_PARAM_TRAITS(device::name, std::string);
XRT_INFO_PARAM_TRAITS(device::nodma, bool);
XRT_INFO_PARAM_TRAITS(device::offline, bool);

} // info
  
class device
{
public:
  /**
   * device() - Constructor for empty device
   */
  device()
  {}

  /**
   * device() - Constructor from device index
   *
   * @param didx
   *  Device index
   *
   * Throws if no device is found matching the specified index.
   */
  XCL_DRIVER_DLLESPEC
  explicit
  device(unsigned int didx);

  /**
   * device() - Constructor from string
   *
   * @param str
   *  String identifying the device to open.  
   *
   * If the string is in BDF format it matched against devices
   * installed on the system.  Otherwise the string is assumed
   * to be a device index.
   * 
   * Throws if string format is invalid or no matching device is
   * found.
   */
  XCL_DRIVER_DLLESPEC
  explicit
  device(const std::string& bdf);

  /// @cond
  /**
   * device() - Constructor from device index
   *
   * @param didx
   *  Device index
   *
   * Provided to resolve ambiguity in conversion from integral
   * to unsigned int.
   */
  explicit
  device(int didx)
    : device(static_cast<unsigned int>(didx))
  {}
  /// @endcond

  /// @cond
  /**
   * device() - Constructor from opaque handle
   *
   * Implementation defined constructor
   */
  explicit
  device(std::shared_ptr<xrt_core::device> hdl)
    : handle(std::move(hdl))
  {}
  /// @endcond

  /**
   * device() - Create a managed device object from a shim xclDeviceHandle
   *
   * @param dhdl
   *  Shim xclDeviceHandle
   * @return
   *  xrt::device object epresenting the opened device, or exception on error
   */
  XCL_DRIVER_DLLESPEC
  explicit
  device(xclDeviceHandle dhdl);

  /**
   * device() - Copy ctor
   */
  device(const device& rhs) = default;

  /**
   * device() - Move ctor
   */
  device(device&& rhs) = default;

  /**
   * operator= () - Move assignment
   */
  device&
  operator=(device&& rhs) = default;

  /**
   * get_info() - Retrieve device parameter information
   *
   * This function is templated on the enumeration value as defined in
   * the enumeration xrt::info::device.
   *
   * The return type of the parameter is based on the instantiated
   * param_traits for the given param enumeration supplied as template
   * argument, see namespace xrt::info
   */
  template <info::device param>
  typename info::param_traits<info::device, param>::return_type
  get_info() const
  {
    return boost::any_cast<
      typename info::param_traits<info::device, param>::return_type  
    >(get_info(param));
  }

  /**
   * load_xclbin() - Load an xclbin 
   *
   * @param xclbin
   *  Pointer to xclbin in memory image
   * @return
   *  UUID of argument xclbin
   */
  XCL_DRIVER_DLLESPEC
  uuid
  load_xclbin(const axlf* xclbin);

  /**
   * load_xclbin() - Read and load an xclbin file
   *
   * @param xclbin_fnm
   *  Full path to xclbin file
   * @return
   *  UUID of argument xclbin
   *
   * This function reads the file from disk and loads
   * the xclbin.   Using this function allows one time
   * allocation of data that needs to be kept in memory.
   */
  XCL_DRIVER_DLLESPEC
  uuid
  load_xclbin(const std::string& xclbin_fnm);

  /**
   * load_xclbin() - load an xclin from an xclbin object
   *
   * @param xclbin
   *  xrt::xclbin object
   * @return
   *  UUID of argument xclbin
   *
   * This function uses the specified xrt::xclbin object created by
   * caller.  The xrt::xclbin object must contain the complete axlf
   * structure.
   */
  XCL_DRIVER_DLLESPEC
  uuid
  load_xclbin(const xrt::xclbin& xclbin);

  /**
   * get_xclbin_uuid() - Get UUID of xclbin image loaded on device
   *
   * @return
   *  UUID of currently loaded xclbin
   *
   * Note that current UUID can be different from the UUID of 
   * the xclbin loaded by this process using load_xclbin()
   */
  XCL_DRIVER_DLLESPEC
  uuid
  get_xclbin_uuid() const;

  /**
   * get_xclbin_section() - Retrieve specified xclbin section
   *
   * @param section
   *  The section to retrieve
   * @param uuid
   *  Xclbin uuid of the xclbin with the section to retrieve
   * @return
   *  The specified section if available.
   *
   * Get the xclbin section of the xclbin currently loaded on the 
   * device.  The function throws on error
   *
   * Note, this API may be replaced with more generic query request access
   */
  template <typename SectionType>
  SectionType
  get_xclbin_section(axlf_section_kind section, const uuid& uuid) const
  {
    return reinterpret_cast<SectionType>(get_xclbin_section(section, uuid).first);
  }

public:
  /// @cond
  /**
   * Undocumented temporary interface during porting
   */
  XCL_DRIVER_DLLESPEC
  operator xclDeviceHandle () const;

  std::shared_ptr<xrt_core::device>
  get_handle() const
  {
    return handle;
  }

  XCL_DRIVER_DLLESPEC void
  reset();

  explicit
  operator bool() const
  {
    return handle != nullptr;
  }
  /// @endcond

private:
  XCL_DRIVER_DLLESPEC
  std::pair<const char*, size_t>
  get_xclbin_section(axlf_section_kind section, const uuid& uuid) const;

  XCL_DRIVER_DLLESPEC
  boost::any
  get_info(info::device param) const;

private:
  std::shared_ptr<xrt_core::device> handle;
};

} // namespace xrt

/// @cond
extern "C" {
#endif

/**
 * xrtDeviceOpen() - Open a device and obtain its handle
 *
 * @index:         Device index
 * Return:         Handle representing the opened device, or nullptr on error
 */
XCL_DRIVER_DLLESPEC
xrtDeviceHandle
xrtDeviceOpen(unsigned int index);

/**
 * xrtDeviceOpenByBDF() - Open a device and obtain its handle
 *
 * @bdf:           PCIe BDF identifying the device to open
 * Return:         Handle representing the opened device, or nullptr on error
 */
XCL_DRIVER_DLLESPEC
xrtDeviceHandle
xrtDeviceOpenByBDF(const char* bdf);

/**
 * xrtDeviceOpenFromXcl() - Open a device from a shim xclDeviceHandle
 *
 * @xhdl:         Shim xclDeviceHandle
 * Return:        Handle representing the opened device, or nullptr on error
 *
 * The returned XRT device handle must be explicitly closed when
 * nolonger needed.
 */
XCL_DRIVER_DLLESPEC
xrtDeviceHandle
xrtDeviceOpenFromXcl(xclDeviceHandle xhdl);

/**
 * xrtDeviceClose() - Close an opened device
 *
 * @dhdl:       Handle to device previously opened with xrtDeviceOpen
 * Return:      0 on success, error otherwise
 */
XCL_DRIVER_DLLESPEC
int
xrtDeviceClose(xrtDeviceHandle dhdl);

/**
 * xrtDeviceLoadXclbin() - Load an xclbin image
 *
 * @dhdl:       Handle to device previously opened with xrtDeviceOpen
 * @xclbin:     Pointer to complete axlf in memory image
 * Return:      0 on success, error otherwise
 *
 * The xclbin image can safely be deleted after calling
 * this funciton.
 */
XCL_DRIVER_DLLESPEC
int
xrtDeviceLoadXclbin(xrtDeviceHandle dhdl, const axlf* xclbin);

/**
 * xrtDeviceLoadXclbinFile() - Read and load an xclbin file
 *
 * @dhdl:       Handle to device previously opened with xrtDeviceOpen
 * @xclbin_fnm: Full path to xclbin file
 * Return:      0 on success, error otherwise
 *
 * This function read the file from disk and loads
 * the xclbin.   Using this function allows one time
 * allocation of data that needs to be kept in memory.
 */
XCL_DRIVER_DLLESPEC
int
xrtDeviceLoadXclbinFile(xrtDeviceHandle dhdl, const char* xclbin_fnm);

/**
 * xrtDeviceLoadXclbinHandle() - load an xclbin from an xrt::xclbin handle
 *
 * @dhdl:       Handle to device previously opened with xrtDeviceOpen
 * @xhdl:       xrt::xclbin handle
 * Return:      0 on success, error otherwise
 *
 * This function uses the specified xrt::xclbin object created by
 * caller.  The xrt::xclbin object must contain the complete axlf
 * structure.
 */
XCL_DRIVER_DLLESPEC
int
xrtDeviceLoadXclbinHandle(xrtDeviceHandle dhdl, xrtXclbinHandle xhdl);

/**
 * xrtDeviceLoadXclbinHandle() - load an xclbin from an xrt::xclbin handle
 *
 * @dhdl:       Handle to device previously opened with xrtDeviceOpen
 * @uuid:       uuid_t struct of xclbin id
 * Return:      0 on success, error otherwise
 *
 * This function reads the xclbin id already loaded in the system and
 * comapres it with the input uuid. If they match, load the cached
 * xclbin metadata into caller's process. Otherwise returns error.
 */
XCL_DRIVER_DLLESPEC
int
xrtDeviceLoadXclbinUUID(xrtDeviceHandle dhdl, const xuid_t uuid);

/**
 * xrtDeviceGetXclbinUUID() - Get UUID of xclbin image loaded on device
 *
 * @dhdl:   Handle to device previously opened with xrtDeviceOpen
 * @out:    Return xclbin id in this uuid_t struct
 * Return:  0 on success or appropriate error number
 *
 * Note that current UUID can be different from the UUID of 
 * the xclbin loaded by this process using @load_xclbin()
 */
XCL_DRIVER_DLLESPEC
int
xrtDeviceGetXclbinUUID(xrtDeviceHandle dhdl, xuid_t out);

/*
 * xrtDeviceToXclDevice() - Undocumented access to shim handle
 */
XCL_DRIVER_DLLESPEC
xclDeviceHandle
xrtDeviceToXclDevice(xrtDeviceHandle dhdl);

/// @endcond
#ifdef __cplusplus
}
#endif

#endif
