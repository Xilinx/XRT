// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv_xocl.h"

namespace {
//Register hw drivers
struct drv_xocl_reg
{
    drv_xocl_reg() {
        auto driver = std::make_shared<xrt_core::pci::drv_xocl>();     
        std::vector<std::shared_ptr<xrt_core::dev>> user_ready_list;
        std::vector<std::shared_ptr<xrt_core::dev>> user_nonready_list;
        driver->scan_devices(user_ready_list, user_nonready_list);
        xrt_core::add_device_list(user_ready_list, /*isuser*/ true, /*isready*/ true);
        xrt_core::add_device_list(user_nonready_list, /*isuser*/ true, /*isready*/ false);
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