// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_xclmgmt.h"

namespace {
//Register hw drivers
struct drv_xclmgmt_reg
{
    drv_xclmgmt_reg() {
        auto driver = std::make_shared<xrt_core::pci::drv_xclmgmt>();
        std::vector<std::shared_ptr<xrt_core::pci::dev>> mgmt_ready_list;
        std::vector<std::shared_ptr<xrt_core::pci::dev>> mgmt_nonready_list;
        driver->scan_devices(mgmt_ready_list, mgmt_nonready_list);
        xrt_core::pci::add_device_list(mgmt_ready_list, /*isuser*/ false, /*isready*/ true);
        xrt_core::pci::add_device_list(mgmt_nonready_list, /*isuser*/ false, /*isready*/ false);     
        std::cout << "drv_xclmgmt registration done" << std::endl;
    }
} drv_xclmgmt_reg;
}

namespace xrt_core { namespace pci {

std::shared_ptr<dev>
drv_xclmgmt::create_pcidev(const std::string& sysfs) const
{
	return std::make_shared<pcidev_linux>(sysfs,  /*isuser*/ false);
}
} 

} // namespace xrt_core :: pci