// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_xclmgmt.h"

namespace {
  //Register hw drivers
  struct drv_xclmgmt_reg
  {
    drv_xclmgmt_reg() {
      auto driver = std::make_shared<xrt_core::pci::drv_xclmgmt>();
      std::vector<std::shared_ptr<xrt_core::dev>> dev_list;
      driver->scan_devices(dev_list);
      xrt_core::register_device_list(dev_list);
      std::cout << "drv_xclmgmt registration done" << std::endl;
    }
  } drv_xclmgmt_reg;
}

namespace xrt_core {
  namespace pci {
    std::shared_ptr<dev>
    drv_xclmgmt::create_pcidev(const std::string& sysfs) const
    {
      return std::make_shared<pcidev_linux>(sysfs,  /*isuser*/ false);
    }
  }
} // namespace xrt_core :: pci
