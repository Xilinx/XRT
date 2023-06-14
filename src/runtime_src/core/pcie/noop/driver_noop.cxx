// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "driver_noop.h"

namespace {
//Register noop driver
struct device_factory_noop_reg
{
  device_factory_noop_reg() 
  {
    auto driver = std::make_shared<xrt_core::noop::driver_noop>();
    std::vector<std::shared_ptr<xrt_core::device_factory>> dev_list;
    driver->scan_devices(dev_list);
    xrt_core::register_device_list(dev_list);
  }
}device_factory_noop_reg;

}

std::shared_ptr<xrt_core::device_factory>
xrt_core::noop::driver_noop::
create_pcidev() const
{
  return std::make_shared<xrt_core::noop::device_factory_noop>(/*isuser*/ true);
}

void
xrt_core::noop::driver_noop::
scan_devices(std::vector<std::shared_ptr<xrt_core::device_factory>>& dev_list) const
{
  try {
    xclProbe();
    auto pf = create_pcidev();
    dev_list.push_back(std::move(pf));
  }
  catch (const std::exception& e) {
    throw std::runtime_error(e.what());
  }
}
