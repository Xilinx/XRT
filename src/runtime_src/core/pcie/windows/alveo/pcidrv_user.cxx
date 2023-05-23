// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_user.h"

namespace {
  //Register windows driver
  struct pcidrv_user_reg
  {
    pcidrv_user_reg() {
      auto driver = std::make_shared<xrt_core::pci::pcidrv_user>();
      std::vector<std::shared_ptr<xrt_core::device_factory>> dev_list;
      driver->scan_devices(dev_list);
      xrt_core::register_device_list(dev_list);
      std::cout << "pcidrv_user registration done" << std::endl;
    }
  } pcidrv_user_reg;
}

namespace xrt_core { namespace pci {
  std::shared_ptr<device_factory>
  pcidrv_user::create_pcidev() const
  {
    return std::make_shared<pcidev_windows>(/*isuser*/ true);
  }

  void
  pcidrv_user::scan_devices(std::vector<std::shared_ptr<device_factory>>& dev_list) const
  {
    try {
      auto nd = xclProbe(); //userpf
      std::cout << "num of windows userpf dev : " << nd << std::endl;
      for (unsigned int idx = 0; idx < nd; idx++) {
        auto pf = create_pcidev();
        dev_list.push_back(std::move(pf));
      }
    }
    catch (const std::exception& e) {
      throw std::runtime_error(e.what());
    }
  }

 } } // namespace xrt_core :: pci
