// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "edgedrv_zocl.h"

namespace {
  //Register sw emu driver
  struct edgedev_linux_reg
  {
    edgedev_linux_reg() {
      auto driver = std::make_shared<xrt_core::pci::edgedrv_zocl>();
      std::vector<std::shared_ptr<xrt_core::dev>> dev_list;
      driver->scan_devices(dev_list);
      xrt_core::register_device_list(dev_list);
      std::cout << "edgedrv_zocl registration done" << std::endl;
    }
  } edgedev_linux_reg;
}

namespace xrt_core {
  namespace edge {
    std::shared_ptr<xrt_core::dev>
    edgedrv_zocl::create_edgedev() const
    {
      return std::make_shared<edgedev_linux>(/*isuser*/ true);
    }

    void
    edgedrv_zocl::scan_devices(std::vector<std::shared_ptr<xrt_core::dev>>& ready_list) const
    {
      try {
        auto nd = xclProbe();
        std::cout << "num zocl dev" << nd << std::endl;
        auto pf = create_edgedev();
        ready_list.push_back(std::move(pf));
      }
      catch (const std::invalid_argument&) {
        //exeception
        std::cout << "************ edgedrv_zocl:scan_devices : exeception **********" << std::endl;
      }
    }

  }
} // namespace xrt_core :: edge
