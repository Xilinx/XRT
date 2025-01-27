// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc.  All rights reserved.
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_ishim_h
#define core_common_ishim_h

#include "error.h"
#include "xcl_graph.h"
#include "xrt.h"

#include "core/common/shim/hwctx_handle.h"
#include "core/include/shim_int.h"
#include "core/include/xdp/counters.h"
#include "core/common/shim/aie_buffer_handle.h"
#include "core/common/shim/graph_handle.h"
#include "core/common/shim/profile_handle.h"

#include "xrt/xrt_aie.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_graph.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_uuid.h"
#include "xrt/experimental/xrt_fence.h"
#include "xrt/experimental/xrt-next.h"

#include <stdexcept>
#include <condition_variable>

// Internal shim function forward declarations
int xclUpdateSchedulerStat(xclDeviceHandle handle);
int xclInternalResetDevice(xclDeviceHandle handle, xclResetKind kind);
int xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t total_size);

namespace xrt_core {

/**
 * struct ishim - Shim API implemented by core libraries
 *
 * All methods throw on error
 */
struct ishim
{
  //
  class not_supported_error : public xrt_core::error
  {
  public:
    explicit
    not_supported_error(const std::string& msg)
      : xrt_core::error{std::errc::not_supported, msg}
    {}
  };

  virtual void
  close_device() = 0;

  // Legacy, to be removed
  virtual void
  open_context(const xrt::uuid& xclbin_uuid, unsigned int ip_index, bool shared) = 0;

  virtual void
  close_context(const xrt::uuid& xclbin_uuid, unsigned int ip_index) = 0;

  virtual void
  reg_read(uint32_t ipidx, uint32_t offset, uint32_t* data) const = 0;

  virtual void
  reg_write(uint32_t ipidx, uint32_t offset, uint32_t data) = 0;

  virtual void
  xread(enum xclAddressSpace addr_space, uint64_t offset, void* buffer, size_t size) const = 0;

  virtual void
  xwrite(enum xclAddressSpace addr_space, uint64_t offset, const void* buffer, size_t size) = 0;

  virtual void
  unmgd_pread(void* buffer, size_t size, uint64_t offset) = 0;

  virtual void
  unmgd_pwrite(const void* buffer, size_t size, uint64_t offset) = 0;

  virtual void
  exec_buf(buffer_handle* boh) = 0;

  virtual int
  exec_wait(int timeout_ms) const = 0;

  virtual void
  load_axlf(const axlf*) = 0;

  virtual void
  reclock(const uint16_t* target_freq_mhz) = 0;

  virtual void
  p2p_enable(bool force) = 0;

  virtual void
  p2p_disable(bool force) = 0;

  virtual void
  set_cma(bool enable, uint64_t size) = 0;

  virtual
  void update_scheduler_status() = 0;

  virtual void
  user_reset(xclResetKind kind) = 0;

  virtual void
  get_device_info(xclDeviceInfo2*)
  { throw not_supported_error{__func__}; }

  virtual size_t
  get_device_timestamp()
  { throw not_supported_error{__func__}; }

  virtual std::string
  get_sysfs_path(const std::string&, const std::string&)
  { throw not_supported_error{__func__}; }

  ////////////////////////////////////////////////////////////////
  // Interfaces for buffer handling
  // Implemented explicitly by concrete shim device class
  ////////////////////////////////////////////////////////////////
  virtual std::unique_ptr<buffer_handle>
  alloc_bo(size_t size, uint64_t flags) = 0;

  virtual std::unique_ptr<buffer_handle>
  alloc_bo(void* userptr, size_t size, uint64_t flags) = 0;

  // Import an exported BO from another process identified by argument pid.
  // This function is only supported on systems with pidfd kernel support
  // Redo when supporting windows
  virtual std::unique_ptr<buffer_handle>
  import_bo(pid_t, shared_handle::export_handle)
  { throw not_supported_error{__func__}; }

  ////////////////////////////////////////////////////////////////
  // Interfaces for fence handling
  ////////////////////////////////////////////////////////////////
  virtual std::unique_ptr<fence_handle>
  create_fence(xrt::fence::access_mode)
  { throw not_supported_error{__func__}; }
  
  virtual std::unique_ptr<fence_handle>
  import_fence(pid_t, shared_handle::export_handle)
  { throw not_supported_error{__func__}; }
  ////////////////////////////////////////////////////////////////
  // Interfaces for hw context handling
  // Implemented explicitly by concrete shim device class
  ////////////////////////////////////////////////////////////////
  // If an xclbin is loaded with load_xclbin, an explicit hw_context
  // cannot be created for that xclbin.  This function throws
  // not_supported_error, if either not implemented or an xclbin
  // was explicitly loaded using load_xclbin
  virtual std::unique_ptr<hwctx_handle>
  create_hw_context(const xrt::uuid& /*xclbin_uuid*/,
                    const xrt::hw_context::cfg_param_type& /*cfg_params*/,
                    xrt::hw_context::access_mode /*mode*/) const = 0;

  // creates hw context using partition size
  // Used in elf flow
  // This function is not supported by all platforms
  virtual std::unique_ptr<hwctx_handle>
  create_hw_context(uint32_t /*partition_size*/,
                    const xrt::hw_context::cfg_param_type& /*cfg_params*/,
                    xrt::hw_context::access_mode /*mode*/) const
  { throw not_supported_error{__func__}; }

  // Registers an xclbin with shim, but does not load it.
  // This is no-op for most platform shims
  virtual void
  register_xclbin(const xrt::xclbin&) const
  { throw not_supported_error{__func__}; }
  ////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////
  // Interface for CU shared read range
  // Implemented explicitly by concrete shim device class
  // 2022.2: Only supported for Alveo Linux
  virtual void
  set_cu_read_range(cuidx_type /*ip_index*/, uint32_t /*start*/, uint32_t /*size*/)
  { throw not_supported_error{__func__}; }
  ////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////
  // Interfaces for custom IP interrupt handling
  // Implemented explicitly by concrete shim device class
  // 2021.1: Only supported for edge shim
  ////////////////////////////////////////////////////////////////
  virtual xclInterruptNotifyHandle
  open_ip_interrupt_notify(unsigned int)
  { throw not_supported_error{__func__}; }

  virtual void
  close_ip_interrupt_notify(xclInterruptNotifyHandle)
  { throw not_supported_error{__func__}; }

  virtual void
  enable_ip_interrupt(xclInterruptNotifyHandle)
  { throw not_supported_error{__func__}; }

  virtual void
  disable_ip_interrupt(xclInterruptNotifyHandle)
  { throw not_supported_error{__func__}; }

  virtual void
  wait_ip_interrupt(xclInterruptNotifyHandle)
  { throw not_supported_error{__func__}; }

  virtual std::cv_status
  wait_ip_interrupt(xclInterruptNotifyHandle, int32_t)
  { throw not_supported_error{__func__}; }

  virtual std::unique_ptr<graph_handle>
  open_graph_handle(const xrt::uuid&, const char*, xrt::graph::access_mode)
  { throw not_supported_error{__func__}; }

  virtual std::unique_ptr<profile_handle>
  open_profile_handle()
  { throw not_supported_error{__func__}; }

  virtual void
  open_aie_context(xrt::aie::access_mode)
  { throw not_supported_error{__func__}; }


  virtual void
  reset_aie()
  { throw not_supported_error{__func__}; }


  virtual void
  wait_gmio(const char*)
  { throw not_supported_error{__func__}; }

  virtual void
  load_axlf_meta(const axlf*)
  { throw not_supported_error{__func__}; }

  virtual std::vector<char>
  read_aie_mem(uint16_t /*col*/, uint16_t /*row*/, uint32_t /*offset*/, uint32_t /*size*/)
  { throw not_supported_error{__func__}; }

  virtual size_t
  write_aie_mem(uint16_t /*col*/, uint16_t /*row*/, uint32_t /*offset*/, const std::vector<char>& /*data*/)
  { throw not_supported_error{__func__}; }

  virtual uint32_t
  read_aie_reg(uint16_t /*col*/, uint16_t /*row*/, uint32_t /*reg_addr*/)
  { throw not_supported_error{__func__}; }

  virtual bool
  write_aie_reg(uint16_t /*col*/, uint16_t /*row*/, uint32_t /*reg_addr*/, uint32_t /*reg_val*/)
  { throw not_supported_error{__func__}; }

  virtual std::unique_ptr<aie_buffer_handle>
  open_aie_buffer_handle(const xrt::uuid&, const char*)
  { throw not_supported_error{__func__}; }

};

template <typename DeviceType>
struct shim : public DeviceType
{
  template <typename ...Args>
  explicit
  shim(Args&&... args)
    : DeviceType(std::forward<Args>(args)...)
  {}

  void
  close_device() override
  {
    xclClose(DeviceType::get_device_handle());
  }

  // Legacy, to be removed
  void
  open_context(const xrt::uuid& xclbin_uuid , unsigned int ip_index, bool shared) override
  {
    if (auto ret = xclOpenContext(DeviceType::get_device_handle(), xclbin_uuid.get(), ip_index, shared))
      throw system_error(ret, "failed to open ip context");
  }

  void
  close_context(const xrt::uuid& xclbin_uuid, unsigned int ip_index) override
  {
    if (auto ret = xclCloseContext(DeviceType::get_device_handle(), xclbin_uuid.get(), ip_index))
      throw system_error(ret, "failed to close ip context");
  }

  void
  reg_read(uint32_t ipidx, uint32_t offset, uint32_t* data) const override
  {
    if (auto ret = xclRegRead(DeviceType::get_device_handle(), ipidx, offset, data))
      throw system_error(ret, "failed to read ip(" + std::to_string(ipidx) + ")");
  }

  void
  reg_write(uint32_t ipidx, uint32_t offset, uint32_t data) override
  {
    if (auto ret = xclRegWrite(DeviceType::get_device_handle(), ipidx, offset, data))
      throw system_error(ret, "failed to write ip(" + std::to_string(ipidx) + ")");
  }

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  void
  xread(enum xclAddressSpace addr_space, uint64_t offset, void* buffer, size_t size) const override
  {
    if (size != xclRead(DeviceType::get_device_handle(), addr_space, offset, buffer, size))
      throw system_error(-1, "failed to read at address (" + std::to_string(offset) + ")");
  }

  void
  xwrite(enum xclAddressSpace addr_space, uint64_t offset, const void* buffer, size_t size) override
  {
    if (size != xclWrite(DeviceType::get_device_handle(), addr_space, offset, buffer, size))
      throw system_error(-1, "failed to write to address (" + std::to_string(offset) + ")");
  }
#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

  void
  unmgd_pread(void* buffer, size_t size, uint64_t offset) override
  {
    if (auto ret = xclUnmgdPread(DeviceType::get_device_handle(), 0, buffer, size, offset))
      throw system_error(static_cast<int>(ret), "failed to read at address (" + std::to_string(offset) + ")");
  }

  void
  unmgd_pwrite(const void* buffer, size_t size, uint64_t offset) override
  {
    if (auto ret = xclUnmgdPwrite(DeviceType::get_device_handle(), 0, buffer, size, offset))
      throw system_error(static_cast<int>(ret), "failed to write to address (" + std::to_string(offset) + ")");
  }

  void
  exec_buf(buffer_handle* bo) override
  {
    if (auto ret = xclExecBuf(DeviceType::get_device_handle(), bo->get_xcl_handle()))
      throw system_error(ret, "failed to launch execution buffer");
  }

  int
  exec_wait(int timeout_ms) const override
  {
    return xclExecWait(DeviceType::get_device_handle(), timeout_ms);
  }

  void
  load_axlf(const axlf* buffer) override
  {
    if (auto ret = xclLoadXclBin(DeviceType::get_device_handle(), buffer))
      throw system_error(ret, "failed to load xclbin");
  }

  void
  reclock(const uint16_t* target_freq_mhz) override
  {
    if (auto ret = xclReClock2(DeviceType::get_device_handle(), 0, target_freq_mhz))
      throw system_error(ret, "failed to reclock specified clock");
  }

  void
  p2p_enable(bool force) override
  {
    if (auto ret = xclP2pEnable(DeviceType::get_device_handle(), true, force))
      throw system_error(ret, "failed to enable p2p");
  }

  void
  p2p_disable(bool force) override
  {
    if (auto ret = xclP2pEnable(DeviceType::get_device_handle(), false, force))
      throw system_error(ret, "failed to disable p2p");
  }

  void
  set_cma(bool enable, uint64_t size) override
  {
    auto ret = xclCmaEnable(DeviceType::get_device_handle(), enable, size);
    if(ret == EXIT_SUCCESS)
      return;
    if(ret == -ENOMEM)
      throw system_error(ret, "Not enough host mem. Please check grub settings.");
    if(ret == -EINVAL)
      throw system_error(ret, "Invalid host mem size. Please specify a memory size between 4M and 1G as a power of 2.");
    if(ret == -ENXIO)
      throw system_error(ret, "Huge page is not supported on this platform");
    if(ret == -ENODEV)
      throw system_error(ret, "Does not support host mem feature");
    if(ret == -EBUSY)
      throw system_error(ret, "Host mem is already enabled or in-use");
    if(ret)
      throw system_error(ret);
  }

  void
  update_scheduler_status() override
  {
    if (auto ret = xclUpdateSchedulerStat(DeviceType::get_device_handle()))
      throw error(ret, "failed to update scheduler status");
  }

  void
  user_reset(xclResetKind kind) override
  {
    if (auto ret = xclInternalResetDevice(DeviceType::get_device_handle(), kind))
      throw error(ret, "failed to reset device");
  }
};

// Stub out all xrt_core::ishim functions to throw not supported. A
// small subset of ishim device level functions are overriden
// and supported by a higher level devices as needed.
template <typename DeviceType>
struct noshim : public DeviceType
{
  template <typename ...Args>
  explicit
  noshim(Args&&... args)
    : DeviceType(std::forward<Args>(args)...)
  {}

  void
  close_device() override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  open_context(const xrt::uuid&, unsigned int, bool) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  close_context(const xrt::uuid&, unsigned int) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  reg_read(uint32_t, uint32_t, uint32_t*) const override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  reg_write(uint32_t, uint32_t, uint32_t) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  xread(enum xclAddressSpace, uint64_t, void*, size_t) const override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  xwrite(enum xclAddressSpace, uint64_t, const void*, size_t) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  unmgd_pread(void*, size_t, uint64_t) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  unmgd_pwrite(const void*, size_t, uint64_t) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  exec_buf(buffer_handle*) override
  {
    throw ishim::not_supported_error(__func__);
  }

  int
  exec_wait(int) const override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  load_axlf(const axlf*) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  reclock(const uint16_t*) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  p2p_enable(bool) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  p2p_disable(bool) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  set_cma(bool, uint64_t) override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  update_scheduler_status() override
  {
    throw ishim::not_supported_error(__func__);
  }

  void
  user_reset(xclResetKind) override
  {
    throw ishim::not_supported_error(__func__);
  }
};

} // xrt_core

#endif
