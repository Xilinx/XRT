// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc. All rights reserved.

#ifndef CORE_PCIE_WINDOWS_MCDM_DEVICE_MCDM_H
#define CORE_PCIE_WINDOWS_MCDM_DEVICE_MCDM_H

#include "core/common/ishim.h"
#include "core/pcie/common/device_pcie.h"

namespace xrt_core {

// concrete class derives from device_pcie, but mixes in
// shim layer functions for access through base class
class device_mcdm : public shim<device_pcie>
{
public:
  // Open an unmanged device.  This ctor is called by xclOpen
  device_mcdm(handle_type device_handle, id_type device_id, bool user);
  ~device_mcdm();

private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const;
};

} // xrt_core

#endif
