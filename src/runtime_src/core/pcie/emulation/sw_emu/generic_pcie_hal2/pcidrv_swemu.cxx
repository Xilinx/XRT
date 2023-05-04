// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_swemu.h"

namespace {
  //Register sw emu driver
  struct pcidev_swemu_reg
  {
    pcidev_swemu_reg() {
      auto driver = std::make_shared<xrt_core::pci::pcidrv_swemu>();
      std::vector<std::shared_ptr<xrt_core::dev>> dev_list;
      driver->scan_devices(dev_list);
      xrt_core::register_device_list(dev_list);
      std::cout << "pcidrv_swemu registration done" << std::endl;
    }
  } pcidev_swemu_reg;
}

namespace xrt_core { namespace pci {
  std::shared_ptr<dev>
  pcidrv_swemu::create_pcidev() const
  {
    return std::make_shared<pcidev_swemu>(/*isuser*/ true);
  }

  void
  pcidrv_swemu::scan_devices(std::vector<std::shared_ptr<dev>>& ready_list) const
  {
    try {
      auto nd = xclProbe();
      std::cout << "num sw_emu dev " << nd << std::endl;
      auto pf = create_pcidev();
      ready_list.push_back(std::move(pf));
    }
    catch (const std::exception& e) {
      throw std::runtime_error(e.what());
    }
  }
} } // namespace xrt_core :: pci
