// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_XOCL_H_
#define _XCL_PCIDRV_XOCL_H_

#include "pcidrv.h"

namespace pcidrv {

class pci_driver_xocl : public pci_driver
{
public:
  const std::string
  name(void) const override
  { return "xocl"; }

  bool
  is_user(void) const override
  { return true; }
};

} // pcidrv

#endif
