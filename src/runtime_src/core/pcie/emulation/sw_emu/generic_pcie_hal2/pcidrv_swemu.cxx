// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_swemu.h"

namespace {
//Register sw emu driver
struct pcidev_swemu_reg
{
	pcidev_swemu_reg() {
        auto driver = std::make_shared<xrt_core::pci::pcidrv_swemu>();
        std::vector<std::shared_ptr<xrt_core::pci::dev>> user_ready_list;
        driver->scan_devices(user_ready_list);
        xrt_core::pci::add_device_list(user_ready_list, /*isuser*/ true, /*isready*/ true);
        std::cout << "pcidrv_swemu registration done" << std::endl;        
   	}
} pcidev_swemu_reg;
}

namespace xrt_core { namespace pci {
std::shared_ptr<dev>
pcidrv_swemu::create_pcidev(const std::string& sysfs) const
{
	return std::make_shared<pcidev_swemu>(/*isuser*/ true);
}

void
pcidrv_swemu::scan_devices(std::vector<std::shared_ptr<dev>>& ready_list) const
{
try {
	auto nd = xclProbe();
	std::cout << "num sw emu dev" << nd << std::endl;
    std::string sysfspath = "";
    auto pf = create_pcidev(sysfspath);
    ready_list.push_back(std::move(pf));
}
catch (const std::invalid_argument&) {
    //exeception
	std::cout << "************ pcidrv__swemu:scan_devices : exeception **********" << std::endl;
}      
}

} } // namespace xrt_core :: pci
