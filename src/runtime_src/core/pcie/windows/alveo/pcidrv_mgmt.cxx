// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_mgmt.h"
#include "mgmt.h"

namespace {
  //Register windows driver
  struct pcidrv_mgmt_reg
  {
    pcidrv_mgmt_reg() {
      auto driver = std::make_shared<xrt_core::pci::pcidrv_mgmt>();
      std::vector<std::shared_ptr<xrt_core::dev>> dev_list;
      driver->scan_devices(dev_list);
      xrt_core::register_device_list(dev_list);
      std::cout << "pcidrv_mgmt registration done" << std::endl;
    }
  } pcidrv_mgmt_reg;
}

namespace xrt_core {
  namespace pci {
    std::shared_ptr<dev>
    pcidrv_mgmt::create_pcidev() const
    {
      return std::make_shared<pcidev_windows>(/*isuser*/ false);
    }

    void
    pcidrv_mgmt::scan_devices(std::vector<std::shared_ptr<dev>>& dev_list) const
    {
      try {
        auto nd = mgmtpf::probe();
        std::cout << "num of windows mgmtpf dev : " << nd << std::endl;
        for (unsigned int idx = 0; idx < nd; idx++) {
          auto pf = create_pcidev();
          dev_list.push_back(std::move(pf));
        }
      }
      catch (const std::invalid_argument&) {
        //exeception
        std::cout << "************ pcidrv_mgmt:scan_devices : exeception **********" << std::endl;
      }
    }

  }
} // namespace xrt_core :: pci
