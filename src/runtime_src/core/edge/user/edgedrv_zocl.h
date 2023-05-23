// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_EDGEDRV_ZOCL_H_
#define _XCL_EDGEDRV_ZOCL_H_

#include "edgedev_linux.h"
#include <string>

namespace xrt_core { namespace edge {
  class edgedrv_zocl
  {
  public:
    std::string
      name() const { return "zocl"; }

    bool
      is_user() const { return true; }

    bool
      is_emulation() const { return false; }

    std::shared_ptr<device_factory>
      create_edgedev() const;

    void
      scan_devices(std::vector<std::shared_ptr<device_factory>>& dev_list) const;
  };
} } // namespace xrt_core :: edge

#endif
