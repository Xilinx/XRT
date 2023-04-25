// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_SWEMU_H_
#define _XCL_PCIDRV_SWEMU_H_

#include "pcidev_swemu.h"
#include <string>

namespace xrt_core { namespace pci {

class pcidrv_swemu
{
public:
  std::string
  name() const { return "swemu"; }

  bool
  is_user() const { return true; }

  bool
  is_emulation() const { return true; }

  std::shared_ptr<dev>
  create_pcidev() const ;

  void
  scan_devices(std::vector<std::shared_ptr<dev>>& dev_list) const;

};

} } // namespace xrt_core :: pci

#endif
