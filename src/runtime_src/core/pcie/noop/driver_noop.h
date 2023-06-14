// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XCL_DRIVER_NOOP_H_
#define XCL_DRIVER_NOOP_H_

#include "device_factory_noop.h"

namespace xrt_core::noop {

class driver_noop
{
public:
  std::string
  name() const { return "noop"; }

  bool
  is_user() const { return true; }

  bool
  is_emulation() const { return false; }

  std::shared_ptr<xrt_core::device_factory>
  create_pcidev() const ;

  void
  scan_devices(std::vector<std::shared_ptr<xrt_core::device_factory>>& dev_list) const;
};

} // xrt_core::noop

#endif
