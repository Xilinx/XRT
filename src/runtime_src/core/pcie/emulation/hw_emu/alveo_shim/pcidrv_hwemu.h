// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_HWEMU_H_
#define _XCL_PCIDRV_HWEMU_H_

#include "pcidev_hwemu.h"
#include <string>

namespace xrt_core { namespace pci {

class pcidrv_hwemu
{
public:
  std::string
  name() const { return "hwemu"; }

  bool
  is_user() const { return true; }

  bool
  is_emulation() const { return true; }

  std::shared_ptr<device_factory>
  create_pcidev() const ;

  void
  scan_devices(std::vector<std::shared_ptr<device_factory>>& dev_list) const;

};

} } // namespace xrt_core :: pci

#endif
