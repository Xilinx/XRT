// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_MGMT_H_
#define _XCL_PCIDRV_MGMT_H_

#include "pcidev_windows.h"
#include <string>

namespace xrt_core { namespace pci {

class pcidrv_mgmt
{
public:
  std::string
  name() const { return "mgmtpf"; }

  bool
  is_user() const { return true; }

  bool
  is_emulation() const { return false; }

  std::shared_ptr<dev>
  create_pcidev() const ;

  void
  scan_devices(std::vector<std::shared_ptr<dev>>& dev_list) const;

};

} } // namespace xrt_core :: pci

#endif
