// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_windows.h"

namespace {
  //Register windows driver
  struct pcidev_windows_reg
  {
    pcidev_windows_reg() {
      auto driver = std::make_shared<xrt_core::pci::pcidrv_windows>();
      std::vector<std::shared_ptr<xrt_core::dev>> dev_list;
      driver->scan_devices(dev_list);
      xrt_core::register_device_list(dev_list);
      std::cout << "pcidrv_windows registration done" << std::endl;
    }
  } pcidev_windows_reg;
}

namespace xrt_core {
  namespace pci {
    std::shared_ptr<dev>
    pcidrv_windows::create_pcidev() const
    {
      return std::make_shared<pcidev_windows>(/*isuser*/ true);
    }

    void
    pcidrv_windows::scan_devices(std::vector<std::shared_ptr<dev>>& dev_list) const
    {
      try {
        auto nd = xclProbe();
        std::cout << "num of windows dev : " << nd << std::endl;
        auto pf = create_pcidev();
        dev_list.push_back(std::move(pf));
      }
      catch (const std::invalid_argument&) {
        //exeception
        std::cout << "************ pcidrv_windows:scan_devices : exeception **********" << std::endl;
      }
    }

  }
} // namespace xrt_core :: pci
