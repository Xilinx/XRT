// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_XCLMGMT_H_
#define _XCL_PCIDRV_XCLMGMT_H_

#include "pcidrv.h"
#include "pcidev_linux.h"
#include <string>

namespace xrt_core { namespace pci {

class drv_xclmgmt : public drv
{
public:
  std::string
  name() const override { return "xclmgmt"; }

  std::shared_ptr<dev>
  create_pcidev(const std::string& sysfs) const override;
};

} } // namespace xrt_core :: pci

#endif
