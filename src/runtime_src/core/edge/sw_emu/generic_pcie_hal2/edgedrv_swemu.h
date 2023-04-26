// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_EDGEDRV_SWEMU_H_
#define _XCL_EDGEDRV_SWEMU_H_

#include "edgedev_swemu.h"
#include <string>

namespace xrt_core {
  namespace edge {

    class edgedrv_swemu
    {
    public:
      std::string
        name() const { return "swemu"; }

      bool
        is_user() const { return true; }

      bool
        is_emulation() const { return true; }

      std::shared_ptr<dev>
        create_edgedev() const;

      void
        scan_devices(std::vector<std::shared_ptr<xrt_core::dev>>& dev_list) const;
    };

  }
} // namespace xrt_core :: edge

#endif
