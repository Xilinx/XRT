// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#ifndef PCIE_DEVICE_LINUX_H
#define PCIE_DEVICE_LINUX_H

#include "core/common/ishim.h"
#include "core/pcie/common/device_pcie.h"

namespace xrt_core {

// concrete class derives from device_pcie, but mixes in
// shim layer functions for access through base class
class device_linux : public shim<device_pcie>
{
public:
  device_linux(handle_type device_handle, id_type device_id, bool user);

  // query functions
  virtual void read_dma_stats(boost::property_tree::ptree& pt) const;

  virtual void read(uint64_t addr, void* buf, uint64_t len) const override;
  virtual void write(uint64_t addr, const void* buf, uint64_t len) const override;
  virtual int  open(const std::string& subdev, int flag) const override;
  virtual void close(int dev_handle) const override;
  virtual void reset(query::reset_type&) const override;
  virtual void xclmgmt_load_xclbin(const char* buffer) const override;
  virtual void device_shutdown() const override;
  virtual void device_online() const override;

public:
  ////////////////////////////////////////////////////////////////
  // Custom ishim implementation
  // Redefined from xrt_core::ishim for functions that are not
  // universally implemented by all shims
  ////////////////////////////////////////////////////////////////
  void
  set_cu_read_range(cuidx_type ip_index, uint32_t start, uint32_t size) override;

  xclInterruptNotifyHandle
  open_ip_interrupt_notify(unsigned int ip_index) override;

  void
  close_ip_interrupt_notify(xclInterruptNotifyHandle handle) override;

  void
  enable_ip_interrupt(xclInterruptNotifyHandle) override;

  void
  disable_ip_interrupt(xclInterruptNotifyHandle) override;

  void
  wait_ip_interrupt(xclInterruptNotifyHandle) override;

  std::cv_status
  wait_ip_interrupt(xclInterruptNotifyHandle, int32_t timeout) override;

  xrt_buffer_handle
  import_bo(pid_t pid, xclBufferExportHandle ehdl) override;

  uint32_t // ctx handle aka slotidx
  create_hw_context(const xrt::uuid& xclbin_uuid,
                    const xrt::hw_context::qos_type& qos,
                    xrt::hw_context::access_mode mode) const override
  {
    return xrt::shim_int::create_hw_context(get_device_handle(), xclbin_uuid, qos, mode);
  }

  void
  destroy_hw_context(uint32_t ctxhdl) const override
  {
    xrt::shim_int::destroy_hw_context(get_device_handle(), ctxhdl);
  }

  void
  register_xclbin(const xrt::xclbin& xclbin) const override
  {
    xrt::shim_int::register_xclbin(get_device_handle(), xclbin);
  }

  // Exec Buf with hw ctx handle.
  void
  exec_buf(xrt_buffer_handle boh, xcl_hwctx_handle ctxhdl) override
  {
      xrt::shim_int::exec_buf(get_device_handle(), boh, ctxhdl);
  }
  ////////////////////////////////////////////////////////////////

private:
  // Private look up function for concrete query::request
  virtual const query::request&
  lookup_query(query::key_type query_key) const override;
};

} // xrt_core

#endif
