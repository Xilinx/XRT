// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_XCLMGMT_H_
#define _XCL_PCIDRV_XCLMGMT_H_

#include "pcidrv.h"
#include <string>

namespace xrt_core { namespace pci {

class drv_xclmgmt : public drv
{
public:
  std::string
  name() const override { return "xclmgmt"; }

  bool
  is_user() const override { return false; }
};

} } // namespace xrt_core :: pci

#endif
