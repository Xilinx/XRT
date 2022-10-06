// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_H_
#define _XCL_PCIDRV_H_

#include "pcidev.h"

namespace xrt_core { namespace pci {

class drv
{
public:
  // Name of the driver as shown under /sys/bus/pci/drivers/
  // The same name should also be driver module name as shown /sys/module
  virtual const std::string
  name() const = 0;

  // If device runs user work load, it is a user pf
  virtual bool
  is_user() const = 0;

  // Scan system, find all supported devices and add them to the list
  void
  scan_devices(std::vector<std::shared_ptr<dev>>& ready_list,
               std::vector<std::shared_ptr<dev>>& nonready_list) const;
private:
  // Create the type of pci::dev driven by this driver which can be added to the list
  virtual std::shared_ptr<xrt_core::pci::dev>
  create_pcidev(const std::string& sysfs) const;
};

} } // namespace xrt_core :: pci

#endif
