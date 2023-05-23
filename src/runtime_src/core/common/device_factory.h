// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _XCL_DEV_FACTORY_H_
#define _XCL_DEV_FACTORY_H_

#include "device.h"
#include <vector>

namespace xrt_core {

// One function on FPGA or AIE device
class device_factory
{
public:
  bool m_is_mgmt = false;
  bool m_is_ready = true; 
  device_factory(bool isuser) {  
      m_is_mgmt = !isuser;  
  }
  
  virtual ~device_factory() {}
 
  // Hand out a "device" instance that is specific to this type of device.
  // Caller will use this device to access device specific implementation of ishim.
  virtual std::shared_ptr<device>
  create_device(device::handle_type handle, device::id_type id) const = 0;

  // Hand out an opaque "shim" handle that is specific to this type of device.
  // On legacy Alveo device, this handle can be used to lookup a device instance and
  // make xcl HAL API calls.
  // On new platforms, this handle can only be used to look up a device. HAL API calls
  // through it are not supported any more.
  virtual device::handle_type
  create_shim(device::id_type id) const = 0;

  //get BDF 
  virtual std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
  get_bdf_info() const
  {
    return std::make_tuple(uint16_t(0), uint16_t(0), uint16_t(0), uint16_t(0));
  }
};

// get list of device_factory objects which are in ready state from registered device list.
size_t
get_device_factory_ready(bool user = true);

// get total number of device_factory objects from registered device list.
size_t
get_device_factory_total(bool user = true);

//get device_factory from list of registered device list.
std::shared_ptr<device_factory>
get_device_factory(unsigned index, bool user = true);


// Adding device_factory instance to the global list. Should only be called from global object of
// built-in drivers through dlopen.
// For now, once added, it cannot be removed until the list itself is out of scope.

XRT_CORE_COMMON_EXPORT
void
register_device_list(const std::vector<std::shared_ptr<xrt_core::device_factory>>& devlist);

} // namespace xrt_core

#endif
