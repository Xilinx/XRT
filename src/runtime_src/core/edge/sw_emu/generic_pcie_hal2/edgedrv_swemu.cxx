// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include "edgedrv_swemu.h"

namespace {
  //Register sw emu driver
  struct edgedev_swemu_reg
  {
    edgedev_swemu_reg() {
      auto driver = std::make_shared<xrt_core::edge::edgedrv_swemu>();
      std::vector<std::shared_ptr<xrt_core::device_factory>> dev_list;
      driver->scan_devices(dev_list);
      xrt_core::register_device_list(dev_list);
      std::cout << "edgedrv_swemu registration done" << std::endl;
    }
  } edgedev_swemu_reg;
}

namespace xrt_core { namespace edge {
  std::shared_ptr<xrt_core::device_factory>
  edgedrv_swemu::create_edgedev() const
  {
    return std::make_shared<edgedev_swemu>(/*isuser*/ true);
  }

  void
  edgedrv_swemu::scan_devices(std::vector<std::shared_ptr<xrt_core::device_factory>>& ready_list) const
  {
    try {
      auto nd = xclProbe();
      std::cout << "num se_emu dev: " << nd << std::endl;
      auto pf = create_edgedev();
      ready_list.push_back(std::move(pf));
    }
    catch (const std::exception& e) {
      throw std::runtime_error(e.what());
    }
  }
} } // namespace xrt_core :: edge
