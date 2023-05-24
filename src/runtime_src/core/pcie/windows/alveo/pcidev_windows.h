// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDEV_WINDOWS_H_
#define _XCL_PCIDEV_WINDOWS_H_

#include "device_windows.h"
#include "core/common/device_factory.h"
#include <string>

namespace xrt_core { namespace pci {

class pcidev_windows : public device_factory
{
public:

  pcidev_windows(bool isuser):device_factory(isuser){}
  // Hand out a "device" instance that is specific to this type of device.
  // Caller will use this device to access device specific implementation of ishim.
  virtual std::shared_ptr<device>
  create_device(device::handle_type handle, device::id_type id) const;

  virtual device::handle_type
  create_shim(device::id_type id) const;

  virtual std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
  get_bdf_info(device::id_type id, bool is_user) const;
};

} } // namespace xrt_core :: pci

#endif
