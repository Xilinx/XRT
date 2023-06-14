// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XCL_DEVICE_FACTORY_NOOP_H_
#define XCL_DEVICE_FACTORY_NOOP_H_

#include "device_noop.h"
#include "core/common/device_factory.h"
#include <string>

namespace xrt_core::noop {

class device_factory_noop : public device_factory
{
public:
  device_factory_noop(bool isuser) : device_factory(isuser)
  {}
  // Hand out a "device" instance that is specific to this type of device.
  // Caller will use this device to access device specific implementation of ishim.
  virtual std::shared_ptr<xrt_core::device>
  create_device(device::handle_type handle, device::id_type id) const override;

  virtual device::handle_type
  create_shim(device::id_type id) const override;
};

} // xrt_core::noop

#endif
