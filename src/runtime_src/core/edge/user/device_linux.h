// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef EDGE_DEVICE_LINUX_H
#define EDGE_DEVICE_LINUX_H

#include "xrt.h"
#include "core/common/ishim.h"
#include "core/common/shim/buffer_handle.h"
#include "core/common/shim/hwctx_handle.h"
#include "core/common/shim/shared_handle.h"
#include "core/edge/common/device_edge.h"

namespace xrt_core {

// concrete class derives from device_edge, but mixes in
// shim layer functions for access through base class
class device_linux : public shim<device_edge>
{
public:
  device_linux(handle_type device_handle, id_type device_id, bool user);

  // query functions
  virtual void read_dma_stats(boost::property_tree::ptree& pt) const;

  virtual void read(uint64_t addr, void* buf, uint64_t len) const;
  virtual void write(uint64_t addr, const void* buf, uint64_t len) const;
  virtual void reset(const query::reset_type) const;

  ////////////////////////////////////////////////////////////////
  // Custom ishim implementation
  // Redefined from xrt_core::ishim for functions that are not
  // universally implemented by all shims
  ////////////////////////////////////////////////////////////////
  void
  set_cu_read_range(cuidx_type ip_index, uint32_t start, uint32_t size) override;

  void
  get_device_info(xclDeviceInfo2 *info) override;

  std::string
  get_sysfs_path(const std::string& subdev, const std::string& entry) override;

  ////////////////////////////////////////////////////////////////
  // Custom ip interrupt handling
  // Redefined from xrt_core::ishim
  ////////////////////////////////////////////////////////////////
  virtual xclInterruptNotifyHandle
  open_ip_interrupt_notify(unsigned int ip_index)
  {
    return xclOpenIPInterruptNotify(get_device_handle(), ip_index, 0);
  }

  virtual void
  close_ip_interrupt_notify(xclInterruptNotifyHandle handle)
  {
    xclCloseIPInterruptNotify(get_device_handle(), handle);
  }

  virtual void
  enable_ip_interrupt(xclInterruptNotifyHandle);

  virtual void
  disable_ip_interrupt(xclInterruptNotifyHandle);

  virtual void
  wait_ip_interrupt(xclInterruptNotifyHandle);

  virtual std::unique_ptr<hwctx_handle>
  create_hw_context(const xrt::uuid& xclbin_uuid,
                    const xrt::hw_context::cfg_param_type& cfg_param,
                    xrt::hw_context::access_mode mode) const override
  {
    return xrt::shim_int::create_hw_context(get_device_handle(), xclbin_uuid, cfg_param, mode);
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
  ////////////////////////////////////////////////////////////////

private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const;
};

}

#endif /* EDGE_DEVICE_LINUX_H */
