// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2020 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _XCL_DEV_FACTORY_H_
#define _XCL_DEV_FACTORY_H_

#include "device.h"

#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace xrt_core {

//// Forward declaration
//class drv;

// One PCIe function on FPGA or AIE device
class dev
{
public:
  bool m_is_mgmt = false;
  bool m_is_ready = true; 
  dev(bool isuser) {  
      m_is_mgmt = !isuser;  
  }
  
  virtual ~dev() {}
 
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


size_t
get_dev_ready(bool user = true);

size_t
get_dev_total(bool user = true);

std::shared_ptr<dev>
get_dev(unsigned index, bool user = true);


/**
 * Adding driver instance to the global list. Should only be called during system_linux's
 * constructor, either explicitly for built-in drivers or through dlopen for plug-in ones.
 * For now, once added, it cannot be removed until the list itself is out of scope.
 */
XRT_CORE_COMMON_EXPORT
void
register_device_list(std::vector<std::shared_ptr<xrt_core::dev>> devlist);

} // namespace xrt_core :: pci

#endif
