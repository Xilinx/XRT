// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_noop.h"

namespace {
//Register noop driver
struct pcidev_noop_reg
{
	pcidev_noop_reg() {
        auto driver = std::make_shared<xrt_core::pci::pcidrv_noop>();
        std::vector<std::shared_ptr<xrt_core::dev>> user_ready_list;
        driver->scan_devices(user_ready_list);
        xrt_core::add_device_list(user_ready_list, /*isuser*/ true, /*isready*/ true);
        std::cout << "pcidrv_noop registration done" << std::endl;        
   	}
} pcidev_noop_reg;
}

namespace xrt_core { namespace pci {
std::shared_ptr<dev>
pcidrv_noop::create_pcidev(const std::string& sysfs) const
{
	return std::make_shared<pcidev_noop>(/*isuser*/ true);
}

void
pcidrv_noop::scan_devices(std::vector<std::shared_ptr<dev>>& ready_list) const
{
try {
	auto nd = xclProbe();
	std::cout << "num noop dev" << nd << std::endl;
    std::string sysfspath = "";
    auto pf = create_pcidev(sysfspath);
    ready_list.push_back(std::move(pf));
}
catch (const std::invalid_argument&) {
    //exeception
	std::cout << "************ pcidrv_noop:scan_devices : exeception **********" << std::endl;
}      
}

} } // namespace xrt_core :: pci
