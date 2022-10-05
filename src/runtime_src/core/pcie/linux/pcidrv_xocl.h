// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_XOCL_H_
#define _XCL_PCIDRV_XOCL_H_

#include "pcidrv.h"

namespace xrt_core { namespace pci {

class drv_xocl : public drv
{
public:
  const std::string
  name(void) const override
  { return "xocl"; }

  bool
  is_user(void) const override
  { return true; }
};

} } // namespace xrt_core :: pci

#endif
