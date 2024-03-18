// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef PCIE_HWEMU_DEVICE_LINUX_H
#define PCIE_HWEMU_DEVICE_LINUX_H
#include "shim_int.h"

#include "core/common/ishim.h"
#include "core/common/shim/buffer_handle.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/common/shim/shared_handle.h"

#include "core/pcie/common/device_pcie.h"
namespace xrt_core { namespace hwemu {

// concrete class derives from device_edge, but mixes in
// shim layer functions for access through base class
class device : public shim<device_pcie>
{
public:
  device(handle_type device_handle, id_type device_id, bool user);

private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const;

  std::unique_ptr<hwctx_handle>
  create_hw_context(const xrt::uuid& xclbin_uuid,
                    const xrt::hw_context::cfg_param_type& cfg_param,
                    xrt::hw_context::access_mode mode) const override
  {
    return xrt::shim_int::create_hw_context(get_device_handle(), xclbin_uuid, cfg_param, mode);
  }

  void
  register_xclbin(const xrt::xclbin& xclbin) const override
  {
    xrt::shim_int::register_xclbin(get_device_handle(), xclbin);
  }

  std::unique_ptr<buffer_handle>
  alloc_bo(size_t size, uint64_t flags) override
  {
    return xrt::shim_int::alloc_bo(get_device_handle(), size, xcl_bo_flags{flags}.flags);
  }

  std::unique_ptr<buffer_handle>
  alloc_bo(void* userptr, size_t size, uint64_t flags) override
  {
    return xrt::shim_int::alloc_bo(get_device_handle(), userptr, size, xcl_bo_flags{flags}.flags);
  }

  std::unique_ptr<buffer_handle>
  import_bo(pid_t pid, shared_handle::export_handle ehdl) override;

  void
  get_device_info(xclDeviceInfo2 *info) override;

  size_t
  get_device_timestamp() override;
};

}} // hwemu, xrt_core

#endif
