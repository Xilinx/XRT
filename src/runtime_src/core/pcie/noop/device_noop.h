// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef PCIE_NOOP_DEVICE_LINUX_H
#define PCIE_NOOP_DEVICE_LINUX_H

#include "core/common/ishim.h"
#include "core/common/query_requests.h"
#include "core/pcie/common/device_pcie.h"

#include "shim.h"

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

  uint32_t // slotidx
  create_hw_context(const xrt::uuid& xclbin_uuid, uint32_t qos) const override
  {
    return userpf::create_hw_context(this, xclbin_uuid, qos);
  }

  void
  destroy_hw_context(uint32_t slotidx) const override
  {
    userpf::destroy_hw_context(this, slotidx);
  }

  void
  register_xclbin(const xrt::xclbin& xclbin) const override
  {
    userpf::register_xclbin(this, xclbin);
  }
};

}} // noop, xrt_core

#endif
