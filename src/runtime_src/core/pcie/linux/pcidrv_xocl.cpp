// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_xocl.h"

namespace {
//Register hw drivers
struct drv_xocl_reg
{
    drv_xocl_reg() {
        auto driver = std::make_shared<xrt_core::pci::drv_xocl>();     
        std::vector<std::shared_ptr<xrt_core::dev>> dev_list;
        driver->scan_devices(dev_list);
        xrt_core::register_device_list(dev_list);
        std::cout << "drv_xocl registration done" << std::endl;
    }
} drv_xocl_reg;
}

namespace xrt_core { namespace pci {

std::shared_ptr<dev>
drv_xocl::create_pcidev(const std::string& sysfs) const
{
    return std::make_shared<pcidev_linux>(sysfs, /*isuser*/ true);
}

} 

} // namespace xrt_core :: pci