// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
#ifndef PCIE_NOOP_DEVICE_LINUX_H
#define PCIE_NOOP_DEVICE_LINUX_H

#include "core/common/ishim.h"
#include "core/common/query_requests.h"
#include "core/pcie/common/device_pcie.h"

namespace xrt_core { namespace noop {

// concrete class derives from device_edge, but mixes in
// shim layer functions for access through base class
class device : public shim<device_pcie>
{
public:
  device(handle_type device_handle, id_type device_id, bool user);

private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const override;
};

}} // noop, xrt_core

#endif
