// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_H_
#define _XCL_PCIDRV_H_

#include "core/common/dev_factory.h"

namespace xrt_core { namespace pci {

class drv
{
public:
  // Name of the driver as shown under /sys/bus/pci/drivers/
  // The same name should also be driver module name as shown /sys/module
  virtual std::string
  name() const = 0;

  // Scan system, find all supported devices and add them to the list
  virtual void
  scan_devices(std::vector<std::shared_ptr<xrt_core::pci::dev>>& ready_list,
               std::vector<std::shared_ptr<xrt_core::pci::dev>>& nonready_list) const;
  // Create the type of pci::dev driven by this driver which can be added to the list
  virtual std::shared_ptr<xrt_core::pci::dev>
  create_pcidev(const std::string& sysfs) const = 0;
};

} } // namespace xrt_core :: pci

#endif
