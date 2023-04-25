// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_hwemu.h"

namespace {
//Register hw emu driver
struct pcidev_hwemu_reg
{
  pcidev_hwemu_reg() {
        auto driver = std::make_shared<xrt_core::pci::pcidrv_hwemu>();
        std::vector<std::shared_ptr<xrt_core::dev>> dev_list;
        driver->scan_devices(dev_list);
        xrt_core::register_device_list(dev_list);
        std::cout << "pcidrv_hwemu registration done" << std::endl;        
   	}
} pcidev_hwemu_reg;
}

namespace xrt_core { namespace pci {

std::shared_ptr<dev>
pcidrv_hwemu::create_pcidev() const
{
	return std::make_shared<pcidev_hwemu>(/*isuser*/ true);
}

void
pcidrv_hwemu::scan_devices(std::vector<std::shared_ptr<dev>>& dev_list) const
{
try {
	auto nd = xclProbe();
	std::cout << "num hw emu dev : " << nd << std::endl;
  auto pf = create_pcidev();
  dev_list.push_back(std::move(pf));
}
catch (const std::invalid_argument&) {
    //exeception
	std::cout << "************ pcidrv_hwemu:scan_devices : exeception **********" << std::endl;
}      
}

} } // namespace xrt_core :: pci
