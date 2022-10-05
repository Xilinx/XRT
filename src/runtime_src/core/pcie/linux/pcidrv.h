// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_H_
#define _XCL_PCIDRV_H_

#include "pcidev.h"

namespace xrt_core { namespace pci {

class drv
{
public:
  virtual
  const std::string
  name(void) const = 0;

  virtual
  bool
  is_user(void) const = 0;

  void
  scan_devices(std::vector<std::shared_ptr<dev>>& ready_list,
               std::vector<std::shared_ptr<dev>>& nonready_list) const;
private:
  virtual
  std::shared_ptr<xrt_core::pci::dev>
  create_pcidev(const std::string& sysfs) const;
};

} } // namespace xrt_core :: pci

#endif
