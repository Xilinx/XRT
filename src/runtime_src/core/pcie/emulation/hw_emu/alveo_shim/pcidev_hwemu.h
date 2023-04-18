// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDEV_HWEMU_H_
#define _XCL_PCIDEV_HWEMU_H_

#include "device_hwemu.h"
#include "core/common/dev_factory.h"
#include <string>

namespace xrt_core { namespace pci {

class pcidev_hwemu : public dev
{
public:

  pcidev_hwemu(bool isuser):dev(isuser){}
  // Hand out a "device" instance that is specific to this type of device.
  // Caller will use this device to access device specific implementation of ishim.
  virtual std::shared_ptr<device>
  create_device(device::handle_type handle, device::id_type id) const override;

  virtual device::handle_type
  create_shim(device::id_type id) const override;
};

} } // namespace xrt_core :: pci

#endif
