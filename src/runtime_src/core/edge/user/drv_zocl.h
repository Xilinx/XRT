// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights

#ifndef DRV_ZOCL_H_
#define DRV_ZOCL_H_

#include <memory>
#include <string>
#include <vector>
#include "drv.h"

namespace xrt_core::edge {

class drv_zocl : public drv
{
public:
   void
   scan_devices(std::vector<std::shared_ptr<dev>>& dev_list) override;

   std::shared_ptr<dev>
   create_edev(const std::string& sysfs="") const override;
};
} //namespace xrt_core::edge
#endif
