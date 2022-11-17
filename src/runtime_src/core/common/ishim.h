// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc.  All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef core_common_ishim_h
#define core_common_ishim_h

#include "error.h"
#include "xcl_graph.h"
#include "xrt.h"
#include "core/include/shim_int.h"

#include "xrt/xrt_aie.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_graph.h"
#include "xrt/xrt_uuid.h"

#include "experimental/xrt_hw_context.h"
#include "experimental/xrt-next.h"

#include <stdexcept>
#include <condition_variable>

// Internal shim function forward declarations
int xclUpdateSchedulerStat(xclDeviceHandle handle);
int xclInternalResetDevice(xclDeviceHandle handle, xclResetKind kind);
int xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t total_size);
int xclCloseExportHandle(xclBufferExportHandle);

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

  virtual xrt_buffer_handle
  alloc_bo(size_t size, unsigned int flags) = 0;

  virtual xrt_buffer_handle
  alloc_bo(void* userptr, size_t size, unsigned int flags) = 0;

  virtual void
  free_bo(xrt_buffer_handle boh) = 0;

  virtual xclBufferExportHandle
  export_bo(xrt_buffer_handle boh) const = 0;

  virtual xrt_buffer_handle
  import_bo(xclBufferExportHandle ehdl) = 0;

  virtual void
  close_export_handle(xclBufferExportHandle) = 0;

  // Import an exported BO from another process identified by argument pid.
  // This function is only supported on systems with pidfd kernel support
  virtual xrt_buffer_handle
  import_bo(pid_t, xclBufferExportHandle)
  { throw not_supported_error{__func__}; }

  virtual void
  copy_bo(xrt_buffer_handle dst, xrt_buffer_handle src, size_t size, size_t dst_offset, size_t src_offset) = 0;

  virtual void
  sync_bo(xrt_buffer_handle bo, xclBOSyncDirection dir, size_t size, size_t offset) = 0;

  virtual void*
  map_bo(xrt_buffer_handle boh, bool write) = 0;

  virtual void
  unmap_bo(xrt_buffer_handle boh, void* addr) = 0;

  virtual void
  get_bo_properties(xrt_buffer_handle boh, struct xclBOProperties *properties) const = 0;

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
  exec_buf(xrt_buffer_handle boh) = 0;

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

  virtual cuidx_type
  open_cu_context(xcl_hwctx_handle, const std::string& /*cuname*/)
  { throw not_supported_error{__func__}; }

  virtual void
  close_cu_context(xcl_hwctx_handle, cuidx_type /*ip_index*/)
  { throw not_supported_error{__func__}; }

  // Deprecated API to be removed when all shims manage a hwctx handle
  virtual cuidx_type
  open_cu_context(const xrt::hw_context&, const std::string& /*cuname*/)
  { throw not_supported_error{__func__}; }

  // Deprecated API to be removed when all shims manage a hwctx handle
  virtual void
  close_cu_context(const xrt::hw_context&, cuidx_type /*ip_index*/)
  { throw not_supported_error{__func__}; }

  // Wrapper used by upper level code to ensure that the handle
  // version of open / close is called if defined. This wrapper
  // is needed because there is no way to get from handle to hwctx.
  cuidx_type
  open_cu_context_wrap(const xrt::hw_context& hwctx, const std::string& cuname)
  {
    try {
      return open_cu_context(static_cast<xcl_hwctx_handle>(hwctx), cuname);
    }
    catch (const not_supported_error&) {
      return open_cu_context(hwctx, cuname);
    }
  }

  void
  close_cu_context_wrap(const xrt::hw_context& hwctx, cuidx_type ip_index)
  {
    try {
      close_cu_context(static_cast<xcl_hwctx_handle>(hwctx), ip_index);
    }
    catch (const not_supported_error&) {
      close_cu_context(hwctx, ip_index);
    }
  }

  ////////////////////////////////////////////////////////////////
  // Interfaces for hw context handling
  // Implemented explicitly by concrete shim device class
  ////////////////////////////////////////////////////////////////
  // If an xclbin is loaded with load_xclbin, an explicit hw_context
  // cannot be created for that xclbin.  This function throws
  // not_supported_error, if either not implemented or an xclbin
  // was explicitly loaded using load_xclbin
  virtual xcl_hwctx_handle
  create_hw_context(const xrt::uuid& /*xclbin_uuid*/, const xrt::hw_context::qos_type& /*qos*/, xrt::hw_context::access_mode /*mode*/) const
  { throw not_supported_error{__func__}; }

  virtual void
  destroy_hw_context(xcl_hwctx_handle /*ctxhdl*/) const
  { throw not_supported_error{__func__}; }

  // Return default sentinel for legacy platforms without hw_queue support
  virtual xcl_hwqueue_handle
  create_hw_queue(xcl_hwctx_handle) const
  { return XRT_NULL_HWQUEUE; }

  // Default noop for legacy platforms without hw_queue support
  virtual void
  destroy_hw_queue(xcl_hwqueue_handle) const
  {}

  // Submits command for execution through hw queue
  virtual void
  submit_command(xcl_hwqueue_handle, xrt_buffer_handle /*cmdbo*/) const
  { throw not_supported_error{__func__}; }

  // Wait for command completion through hw queue
  // Returns 0 on timeout else a value that indicates specified
  // cmdbo completed.  If cmdbo is XRT_NULL_BO then function must
  // returns when some previously submitted command completes.
  virtual int
  wait_command(xcl_hwqueue_handle, xrt_buffer_handle /*cmdbo*/, int /*timeout_ms*/) const
  { throw not_supported_error{__func__}; }

  // Registers an xclbin with shim, but does not load it.
  // This is no-op for most platform shims
  virtual void
  register_xclbin(const xrt::xclbin&) const
  { throw not_supported_error{__func__}; }

  // Allocate a bo within ctx.  This is opt-in, currently reverts to
  // legacy alloc_bo
  virtual xrt_buffer_handle
  alloc_bo(xcl_hwctx_handle, size_t size, unsigned int flags)
  {
    return alloc_bo(size, flags);
  }

  // Allocate a userptr bo within ctx.  This is opt-in, currently
  // reverts to legacy alloc_bo
  virtual xrt_buffer_handle
  alloc_bo(xcl_hwctx_handle, void* userptr, size_t size, unsigned int flags)
  {
    return alloc_bo(userptr, size, flags);
  }

  // Execute a command bo within a ctx.  This is opt-in,  if not supported, then
  // just call legacy exec_buf without the hardware context.
  virtual void
  exec_buf(xrt_buffer_handle boh, xcl_hwctx_handle /*ctxhdl*/)
  {
    exec_buf(boh);
  }
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
  ////////////////////////////////////////////////////////////////

#ifdef XRT_ENABLE_AIE
  virtual xclGraphHandle
  open_graph(const xrt::uuid&, const char*, xrt::graph::access_mode am) = 0;

  virtual void
  close_graph(xclGraphHandle handle) = 0;

  virtual void
  reset_graph(xclGraphHandle handle) = 0;

  virtual uint64_t
  get_timestamp(xclGraphHandle handle) = 0;

  virtual void
  run_graph(xclGraphHandle handle, int iterations) = 0;

  virtual int
  wait_graph_done(xclGraphHandle handle, int timeout) = 0;

  virtual void
  wait_graph(xclGraphHandle handle, uint64_t cycle) = 0;

  virtual void
  suspend_graph(xclGraphHandle handle) = 0;

  virtual void
  resume_graph(xclGraphHandle handle) = 0;

  virtual void
  end_graph(xclGraphHandle handle, uint64_t cycle) = 0;

  virtual void
  update_graph_rtp(xclGraphHandle handle, const char* port, const char* buffer, size_t size) = 0;

  virtual void
  read_graph_rtp(xclGraphHandle handle, const char* port, char* buffer, size_t size) = 0;

  virtual void
  open_aie_context(xrt::aie::access_mode) = 0;

  virtual void
  sync_aie_bo(xrt::bo& bo, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset) = 0;

  virtual void
  reset_aie() = 0;

  virtual void
  sync_aie_bo_nb(xrt::bo& bo, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset) = 0;

  virtual void
  wait_gmio(const char *gmioName) = 0;

  virtual int
  start_profiling(int option, const char* port1Name, const char* port2Name, uint32_t value) = 0;

  virtual uint64_t
  read_profiling(int phdl) = 0;

  virtual void
  stop_profiling(int phdl) = 0;

  virtual void
  load_axlf_meta(const axlf*) = 0;
#endif
};

template <typename DeviceType>
struct shim : public DeviceType
{
  template <typename ...Args>
  shim(Args&&... args)
    : DeviceType(std::forward<Args>(args)...)
  {}

  void
  close_device() override
  {
    xclClose(DeviceType::get_device_handle());
  }

  cuidx_type
  open_cu_context(const xrt::hw_context& hwctx, const std::string& cuname) override
  {
    return xrt::shim_int::open_cu_context(DeviceType::get_device_handle(), hwctx, cuname);
  }

  void
  close_cu_context(const xrt::hw_context& hwctx, cuidx_type cuidx) override
  {
    xrt::shim_int::close_cu_context(DeviceType::get_device_handle(), hwctx, cuidx);
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

  xrt_buffer_handle
  alloc_bo(size_t size, unsigned int flags) override
  {
    auto bo = xclAllocBO(DeviceType::get_device_handle(), size, 0, flags);
    if (bo == XRT_NULL_BO)
      throw std::bad_alloc();

    return to_xrt_buffer_handle(bo);
  }

  xrt_buffer_handle
  alloc_bo(void* userptr, size_t size, unsigned int flags) override
  {
    auto bo = xclAllocUserPtrBO(DeviceType::get_device_handle(), userptr, size, flags);
    if (bo == XRT_NULL_BO)
      throw std::bad_alloc();

    return to_xrt_buffer_handle(bo);
  }

  void
  free_bo(xrt_buffer_handle bo) override
  {
    xclFreeBO(DeviceType::get_device_handle(), to_xclBufferHandle(bo));
  }

  xclBufferExportHandle
  export_bo(xrt_buffer_handle bo) const override
  {
    auto ehdl = xclExportBO(DeviceType::get_device_handle(), to_xclBufferHandle(bo));
    if (ehdl == XRT_NULL_BO_EXPORT)
      throw system_error(EINVAL, "Unable to export BO: bad export BO handle");
    if (ehdl < 0) // system error code
      throw system_error(ENODEV, "Unable to export BO");
    return ehdl;
  }

  xrt_buffer_handle
  import_bo(xclBufferExportHandle ehdl) override
  {
    auto ihdl = xclImportBO(DeviceType::get_device_handle(), ehdl, 0);
    if (ihdl == XRT_NULL_BO)
      throw system_error(EINVAL, "unable to import BO: bad BO handle");
    if (ihdl < 0) // system error code
      throw system_error(ENODEV, "unable to import BO");
    return to_xrt_buffer_handle(ihdl);
  }

  void
  close_export_handle(xclBufferExportHandle ehdl) override
  {
    if (auto err = xclCloseExportHandle(ehdl))
      throw system_error(err, "failed to close export handle");
  }

  void
  copy_bo(xrt_buffer_handle dst, xrt_buffer_handle src, size_t size, size_t dst_offset, size_t src_offset) override
  {
    auto err = xclCopyBO(DeviceType::get_device_handle(),
      to_xclBufferHandle(dst), to_xclBufferHandle(src), size, dst_offset, src_offset);
    if (err)
      throw system_error(err, "unable to copy BO");
  }

  void
  sync_bo(xrt_buffer_handle bo, xclBOSyncDirection dir, size_t size, size_t offset) override
  {
    auto err = xclSyncBO(DeviceType::get_device_handle(), to_xclBufferHandle(bo), dir, size, offset);
    if (err)
      throw system_error(err, "unable to sync BO");
  }

  void*
  map_bo(xrt_buffer_handle bo, bool write) override
  {
    if (auto mapped = xclMapBO(DeviceType::get_device_handle(), to_xclBufferHandle(bo), write))
      return mapped;
    throw system_error(EINVAL, "could not map BO");
  }

  void
  unmap_bo(xrt_buffer_handle bo, void* addr) override
  {
    if (auto ret = xclUnmapBO(DeviceType::get_device_handle(), to_xclBufferHandle(bo), addr))
      throw system_error(ret, "failed to unmap BO");
  }

  void
  get_bo_properties(xrt_buffer_handle bo, struct xclBOProperties *properties) const override
  {
    if (auto ret = xclGetBOProperties(DeviceType::get_device_handle(), to_xclBufferHandle(bo), properties))
      throw system_error(ret, "failed to get BO properties");
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
  exec_buf(xrt_buffer_handle bo) override
  {
    if (auto ret = xclExecBuf(DeviceType::get_device_handle(), to_xclBufferHandle(bo)))
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

#ifdef XRT_ENABLE_AIE
  xclGraphHandle
  open_graph(const xrt::uuid& uuid, const char *gname, xrt::graph::access_mode am) override
  {
    if (auto ghdl = xclGraphOpen(DeviceType::get_device_handle(), uuid.get(), gname, am))
      return ghdl;

    throw system_error(EINVAL, "failed to open graph");
  }

  void
  close_graph(xclGraphHandle handle) override
  {
    return xclGraphClose(handle);
  }

  void
  reset_graph(xclGraphHandle handle) override
  {
    if (auto ret = xclGraphReset(handle))
      throw system_error(ret, "fail to reset graph");
  }

  uint64_t
  get_timestamp(xclGraphHandle handle) override
  {
    return xclGraphTimeStamp(handle);
  }

  void
  run_graph(xclGraphHandle handle, int iterations) override
  {
    if (auto ret = xclGraphRun(handle, iterations))
      throw system_error(ret, "fail to run graph");
  }

  int
  wait_graph_done(xclGraphHandle handle, int timeout) override
  {
    return xclGraphWaitDone(handle, timeout);
  }

  void
  wait_graph(xclGraphHandle handle, uint64_t cycle) override
  {
    if (auto ret = xclGraphWait(handle, cycle))
      throw system_error(ret, "fail to wait graph");
  }

  void
  suspend_graph(xclGraphHandle handle) override
  {
    if (auto ret = xclGraphSuspend(handle))
      throw system_error(ret, "fail to suspend graph");
  }

  void
  resume_graph(xclGraphHandle handle) override
  {
    if (auto ret = xclGraphResume(handle))
      throw system_error(ret, "fail to resume graph");
  }

  void
  end_graph(xclGraphHandle handle, uint64_t cycle) override
  {
    if (auto ret = xclGraphEnd(handle, cycle))
      throw system_error(ret, "fail to end graph");
  }

  void
  update_graph_rtp(xclGraphHandle handle, const char* port, const char* buffer, size_t size) override
  {
    if (auto ret = xclGraphUpdateRTP(handle, port, buffer, size))
      throw system_error(ret, "fail to update graph rtp");
  }

  void
  read_graph_rtp(xclGraphHandle handle, const char* port, char* buffer, size_t size) override
  {
    if (auto ret = xclGraphReadRTP(handle, port, buffer, size))
      throw system_error(ret, "fail to read graph rtp");
  }

  void
  open_aie_context(xrt::aie::access_mode am) override
  {
    if (auto ret = xclAIEOpenContext(DeviceType::get_device_handle(), am))
      throw error(ret, "fail to open aie context");
  }

  void
  sync_aie_bo(xrt::bo& bo, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset) override
  {
    if (auto ret = xclSyncBOAIE(DeviceType::get_device_handle(), bo, gmioName, dir, size, offset))
      throw system_error(ret, "fail to sync aie bo");
  }

  void
  reset_aie() override
  {
    if (auto ret = xclResetAIEArray(DeviceType::get_device_handle()))
      throw system_error(ret, "fail to reset aie");
  }

  void
  sync_aie_bo_nb(xrt::bo& bo, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset) override
  {
    if (auto ret = xclSyncBOAIENB(DeviceType::get_device_handle(), bo, gmioName, dir, size, offset))
      throw system_error(ret, "fail to sync aie non-blocking bo");
  }

  void
  wait_gmio(const char *gmioName) override
  {
    if (auto ret = xclGMIOWait(DeviceType::get_device_handle(), gmioName))
      throw system_error(ret, "fail to wait gmio");
  }

  int
  start_profiling(int option, const char* port1Name, const char* port2Name, uint32_t value) override
  {
    return xclStartProfiling(DeviceType::get_device_handle(), option, port1Name, port2Name, value);
  }

  uint64_t
  read_profiling(int phdl) override
  {
    return xclReadProfiling(DeviceType::get_device_handle(), phdl);
  }

  void
  stop_profiling(int phdl) override
  {
    if (auto ret = xclStopProfiling(DeviceType::get_device_handle(), phdl))
      throw system_error(ret, "fail to wait gmio");
  }

  void
  load_axlf_meta(const axlf* buffer) override
  {
    if (auto ret = xclLoadXclBinMeta(DeviceType::get_device_handle(), buffer))
      throw system_error(ret, "failed to load xclbin");
  }
#endif
};

} // xrt_core

#endif
