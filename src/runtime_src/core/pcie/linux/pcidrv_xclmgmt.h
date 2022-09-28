// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_XCLMGMT_H_
#define _XCL_PCIDRV_XCLMGMT_H_

#include "pcidrv_xocl.h"

namespace pcidrv {

class pci_driver_xclmgmt : public pci_driver_xocl
{
public:
  const std::string
  name(void) const override
  { return "xclmgmt"; }

  bool
  is_user(void) const override
  { return false; }

};

} // pcidrv

#endif
