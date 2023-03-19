// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef PCIE_SWEMU_DEVICE_LINUX_H
#define PCIE_SWEMU_DEVICE_LINUX_H
#include "shim_int.h"

#include "core/common/ishim.h"
#include "core/common/shim/buffer_handle.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/common/shim/shared_handle.h"

#include "core/pcie/common/device_pcie.h"

namespace xrt_core { namespace swemu {

// concrete class derives from device_edge, but mixes in
// shim layer functions for access through base class
class device : public shim<device_pcie>
{
public:
  device(handle_type device_handle, id_type device_id, bool user);

  std::unique_ptr<hwctx_handle>
  create_hw_context(const xrt::uuid& xclbin_uuid,
                    const xrt::hw_context::cfg_param_type& cfg_param,
                    xrt::hw_context::access_mode mode) const override
  {
    return xrt::shim_int::create_hw_context(get_device_handle(), xclbin_uuid, cfg_param, mode);
  }

  std::unique_ptr<buffer_handle>
  alloc_bo(size_t size, unsigned int flags) override
  {
    return xrt::shim_int::alloc_bo(get_device_handle(), size, flags);
  }

  std::unique_ptr<buffer_handle>
  alloc_bo(void* userptr, size_t size, unsigned int flags) override
  {
    return xrt::shim_int::alloc_bo(get_device_handle(), userptr, size, flags);
  }

  std::unique_ptr<buffer_handle>
  import_bo(pid_t pid, shared_handle::export_handle ehdl) override;

private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const;
};

}} // swemu, xrt_core

#endif
