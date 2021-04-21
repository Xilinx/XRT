/*
 * Copyright (C) 2019-2021 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef core_common_ishim_h
#define core_common_ishim_h

#include "xrt.h"
#include "experimental/xrt_aie.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_graph.h"
#include "experimental/xrt-next.h"
#include "xcl_graph.h"
#include "error.h"
#include <stdexcept>

// Internal shim function forward declarations
int xclUpdateSchedulerStat(xclDeviceHandle handle);
int xclInternalResetDevice(xclDeviceHandle handle, xclResetKind kind);

namespace xrt_core {

/**
 * struct ishim - Shim API implemented by core libraries
 *
 * All methods throw on error
 */
struct ishim
{
  virtual void
  close_device() = 0;

  virtual void
  open_context(const xuid_t xclbin_uuid, unsigned int ip_index, bool shared) = 0;

  virtual void
  close_context(const xuid_t xclbin_uuid, unsigned int ip_index) = 0;

  virtual xclBufferHandle
  alloc_bo(size_t size, unsigned int flags) = 0;

  virtual xclBufferHandle
  alloc_bo(void* userptr, size_t size, unsigned int flags) = 0;

  virtual void
  free_bo(xclBufferHandle boh) = 0;

  virtual xclBufferExportHandle
  export_bo(xclBufferHandle boh) const = 0;

  virtual xclBufferHandle
  import_bo(xclBufferExportHandle ehdl) = 0;

  virtual void
  copy_bo(xclBufferHandle dst, xclBufferHandle src, size_t size, size_t dst_offset, size_t src_offset) = 0;

  virtual void
  sync_bo(xclBufferHandle bo, xclBOSyncDirection dir, size_t size, size_t offset) = 0;

  virtual void*
  map_bo(xclBufferHandle boh, bool write) = 0;

  virtual void
  unmap_bo(xclBufferHandle boh, void* addr) = 0;

  virtual void
  get_bo_properties(xclBufferHandle boh, struct xclBOProperties *properties) const = 0;

  virtual void
  reg_read(uint32_t ipidx, uint32_t offset, uint32_t* data) const = 0;

  virtual void
  reg_write(uint32_t ipidx, uint32_t offset, uint32_t data) = 0;

  virtual void
  xread(uint64_t offset, void* buffer, size_t size) const = 0;

  virtual void
  xwrite(uint64_t offset, const void* buffer, size_t size) = 0;

  virtual void
  unmgd_pread(void* buffer, size_t size, uint64_t offset) = 0;

  virtual void
  unmgd_pwrite(const void* buffer, size_t size, uint64_t offset) = 0;

  virtual void
  exec_buf(xclBufferHandle boh) = 0;

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

  virtual
  void update_scheduler_status() = 0;

  virtual void
  user_reset(xclResetKind kind) = 0;

  ////////////////////////////////////////////////////////////////
  // Interfaces for custom IP interrupt handling
  // Implemented explicitly by concrete shim device class
  // 2021.1: Only supported for edge shim
  ////////////////////////////////////////////////////////////////
  virtual xclInterruptNotifyHandle
  open_ip_interrupt_notify(unsigned int)
  { throw xrt_core::error(std::errc::not_supported,"open_ip_interrupt_notify()"); }

  virtual void
  close_ip_interrupt_notify(xclInterruptNotifyHandle)
  { throw xrt_core::error(std::errc::not_supported,"close_ip_interrupt_notify()"); }

  virtual void
  enable_ip_interrupt(xclInterruptNotifyHandle)
  { throw xrt_core::error(std::errc::not_supported,"enable_ip_interrupt()"); }

  virtual void
  disable_ip_interrupt(xclInterruptNotifyHandle)
  { throw xrt_core::error(std::errc::not_supported,"disable_ip_interrupt()"); }

  virtual void
  wait_ip_interrupt(xclInterruptNotifyHandle)
  { throw xrt_core::error(std::errc::not_supported,"wait_ip_interrupt()"); }
  ////////////////////////////////////////////////////////////////

#ifdef XRT_ENABLE_AIE
  virtual xclGraphHandle
  open_graph(const xuid_t, const char*, xrt::graph::access_mode am) = 0;

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

  virtual void
  close_device()
  {
    xclClose(DeviceType::get_device_handle());
  }

  virtual void
  open_context(const xuid_t xclbin_uuid , unsigned int ip_index, bool shared)
  {
    if (auto ret = xclOpenContext(DeviceType::get_device_handle(), xclbin_uuid, ip_index, shared))
      throw system_error(ret, "failed to open ip context");
  }

  virtual void
  close_context(const xuid_t xclbin_uuid, unsigned int ip_index)
  {
    if (auto ret = xclCloseContext(DeviceType::get_device_handle(), xclbin_uuid, ip_index))
      throw system_error(ret, "failed to close ip context");
  }

  virtual xclBufferHandle
  alloc_bo(size_t size, unsigned int flags)
  {
    auto bo = xclAllocBO(DeviceType::get_device_handle(), size, 0, flags);
    if (bo == XRT_NULL_BO)
      throw std::bad_alloc();

    return bo;
  }

  virtual xclBufferHandle
  alloc_bo(void* userptr, size_t size, unsigned int flags)
  {
    auto bo = xclAllocUserPtrBO(DeviceType::get_device_handle(), userptr, size, flags);
    if (bo == XRT_NULL_BO)
      throw std::bad_alloc();

    return bo;
  }

  virtual void
  free_bo(xclBufferHandle bo)
  {
    xclFreeBO(DeviceType::get_device_handle(), bo);
  }

  virtual xclBufferExportHandle
  export_bo(xclBufferHandle bo) const
  {
    auto ehdl = xclExportBO(DeviceType::get_device_handle(), bo);
    if (ehdl == XRT_NULL_BO_EXPORT)
      throw system_error(EINVAL, "Unable to export BO");
    if (ehdl < 0) // system error code
      throw system_error(ENODEV, "Unable to export BO");
    return ehdl;
  }

  virtual xclBufferHandle
  import_bo(xclBufferExportHandle ehdl)
  {
    auto ihdl = xclImportBO(DeviceType::get_device_handle(), ehdl, 0);
    if (ihdl == XRT_NULL_BO)
      throw system_error(EINVAL, "unable to import BO");
    if (ihdl < 0) // system error code
      throw system_error(ENODEV, "unable to import BO");
    return ihdl;
  }

  virtual void
  copy_bo(xclBufferHandle dst, xclBufferHandle src, size_t size, size_t dst_offset, size_t src_offset)
  {
    if (auto err = xclCopyBO(DeviceType::get_device_handle(), dst, src, size, dst_offset, src_offset))
      throw system_error(err, "unable to copy BO");
  }

  virtual void
  sync_bo(xclBufferHandle bo, xclBOSyncDirection dir, size_t size, size_t offset)
  {
    if (auto err = xclSyncBO(DeviceType::get_device_handle(), bo, dir, size, offset))
      throw system_error(err, "unable to sync BO");
  }

  virtual void*
  map_bo(xclBufferHandle bo, bool write)
  {
    if (auto mapped = xclMapBO(DeviceType::get_device_handle(), bo, write))
      return mapped;
    throw system_error(EINVAL, "could not map BO");
  }

  virtual void
  unmap_bo(xclBufferHandle bo, void* addr)
  {
    if (auto ret = xclUnmapBO(DeviceType::get_device_handle(), bo, addr))
      throw system_error(ret, "failed to unmap BO");
  }

  virtual void
  get_bo_properties(xclBufferHandle bo, struct xclBOProperties *properties) const
  {
    if (auto ret = xclGetBOProperties(DeviceType::get_device_handle(), bo, properties))
      throw system_error(ret, "failed to get BO properties");
  }

  virtual void
  reg_read(uint32_t ipidx, uint32_t offset, uint32_t* data) const
  {
    if (auto ret = xclRegRead(DeviceType::get_device_handle(), ipidx, offset, data))
      throw system_error(ret, "failed to read ip(" + std::to_string(ipidx) + ")");
  }

  virtual void
  reg_write(uint32_t ipidx, uint32_t offset, uint32_t data)
  {
    if (auto ret = xclRegWrite(DeviceType::get_device_handle(), ipidx, offset, data))
      throw system_error(ret, "failed to write ip(" + std::to_string(ipidx) + ")");
  }

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  virtual void
  xread(uint64_t offset, void* buffer, size_t size) const
  {
    if (size != xclRead(DeviceType::get_device_handle(), XCL_ADDR_KERNEL_CTRL, offset, buffer, size))
      throw system_error(-1, "failed to read at address (" + std::to_string(offset) + ")");
  }

  virtual void
  xwrite(uint64_t offset, const void* buffer, size_t size)
  {
    if (size != xclWrite(DeviceType::get_device_handle(), XCL_ADDR_KERNEL_CTRL, offset, buffer, size))
      throw system_error(-1, "failed to write to address (" + std::to_string(offset) + ")");
  }
#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

  virtual void
  unmgd_pread(void* buffer, size_t size, uint64_t offset)
  {
    if (auto ret = xclUnmgdPread(DeviceType::get_device_handle(), 0, buffer, size, offset))
      throw system_error(static_cast<int>(ret), "failed to read at address (" + std::to_string(offset) + ")");
  }

  virtual void
  unmgd_pwrite(const void* buffer, size_t size, uint64_t offset)
  {
    if (auto ret = xclUnmgdPwrite(DeviceType::get_device_handle(), 0, buffer, size, offset))
      throw system_error(static_cast<int>(ret), "failed to write to address (" + std::to_string(offset) + ")");
  }

  virtual void
  exec_buf(xclBufferHandle bo)
  {
    if (auto ret = xclExecBuf(DeviceType::get_device_handle(), bo))
      throw system_error(ret, "failed to launch execution buffer");
  }

  virtual int
  exec_wait(int timeout_ms) const
  {
    return xclExecWait(DeviceType::get_device_handle(), timeout_ms);
  }

  virtual void
  load_axlf(const axlf* buffer)
  {
    if (auto ret = xclLoadXclBin(DeviceType::get_device_handle(), buffer))
      throw system_error(ret, "failed to load xclbin");
  }

  virtual void
  reclock(const uint16_t* target_freq_mhz)
  {
    if (auto ret = xclReClock2(DeviceType::get_device_handle(), 0, target_freq_mhz))
      throw system_error(ret, "failed to reclock specified clock");
  }

  virtual void
  p2p_enable(bool force)
  {
    if (auto ret = xclP2pEnable(DeviceType::get_device_handle(), true, force))
      throw system_error(ret, "failed to enable p2p");
  }

  virtual void
  p2p_disable(bool force)
  {
    if (auto ret = xclP2pEnable(DeviceType::get_device_handle(), false, force))
      throw system_error(ret, "failed to disable p2p");
  }

  virtual void
  update_scheduler_status()
  {
    if (auto ret = xclUpdateSchedulerStat(DeviceType::get_device_handle()))
      throw error(ret, "failed to update scheduler status");
  }

  virtual void
  user_reset(xclResetKind kind)
  {
    if (auto ret = xclInternalResetDevice(DeviceType::get_device_handle(), kind))
      throw error(ret, "failed to reset device");
  }

#ifdef XRT_ENABLE_AIE
  virtual xclGraphHandle
  open_graph(const xuid_t uuid, const char *gname, xrt::graph::access_mode am)
  {
    if (auto ghdl = xclGraphOpen(DeviceType::get_device_handle(), uuid, gname, am))
      return ghdl;

    throw system_error(EINVAL, "failed to open graph");
  }

  virtual void
  close_graph(xclGraphHandle handle)
  {
    return xclGraphClose(handle);
  }

  virtual void
  reset_graph(xclGraphHandle handle)
  {
    if (auto ret = xclGraphReset(handle))
      throw system_error(ret, "fail to reset graph");
  }

  virtual uint64_t
  get_timestamp(xclGraphHandle handle)
  {
    return xclGraphTimeStamp(handle);
  }

  virtual void
  run_graph(xclGraphHandle handle, int iterations)
  {
    if (auto ret = xclGraphRun(handle, iterations))
      throw system_error(ret, "fail to run graph");
  }

  virtual int
  wait_graph_done(xclGraphHandle handle, int timeout)
  {
    return xclGraphWaitDone(handle, timeout);
  }

  virtual void
  wait_graph(xclGraphHandle handle, uint64_t cycle)
  {
    if (auto ret = xclGraphWait(handle, cycle))
      throw system_error(ret, "fail to wait graph");
  }

  virtual void
  suspend_graph(xclGraphHandle handle)
  {
    if (auto ret = xclGraphSuspend(handle))
      throw system_error(ret, "fail to suspend graph");
  }

  virtual void
  resume_graph(xclGraphHandle handle)
  {
    if (auto ret = xclGraphResume(handle))
      throw system_error(ret, "fail to resume graph");
  }

  virtual void
  end_graph(xclGraphHandle handle, uint64_t cycle)
  {
    if (auto ret = xclGraphEnd(handle, cycle))
      throw system_error(ret, "fail to end graph");
  }

  virtual void
  update_graph_rtp(xclGraphHandle handle, const char* port, const char* buffer, size_t size)
  {
    if (auto ret = xclGraphUpdateRTP(handle, port, buffer, size))
      throw system_error(ret, "fail to update graph rtp");
  }

  virtual void
  read_graph_rtp(xclGraphHandle handle, const char* port, char* buffer, size_t size)
  {
    if (auto ret = xclGraphReadRTP(handle, port, buffer, size))
      throw system_error(ret, "fail to read graph rtp");
  }

  virtual void
  open_aie_context(xrt::aie::access_mode am)
  {
    if (auto ret = xclAIEOpenContext(DeviceType::get_device_handle(), am))
      throw error(ret, "fail to open aie context");
  }

  virtual void
  sync_aie_bo(xrt::bo& bo, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset)
  {
    if (auto ret = xclSyncBOAIE(DeviceType::get_device_handle(), bo, gmioName, dir, size, offset))
      throw system_error(ret, "fail to sync aie bo");
  }

  virtual void
  reset_aie()
  {
    if (auto ret = xclResetAIEArray(DeviceType::get_device_handle()))
      throw system_error(ret, "fail to reset aie");
  }

  virtual void
  sync_aie_bo_nb(xrt::bo& bo, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset)
  {
    if (auto ret = xclSyncBOAIENB(DeviceType::get_device_handle(), bo, gmioName, dir, size, offset))
      throw system_error(ret, "fail to sync aie non-blocking bo");
  }

  virtual void
  wait_gmio(const char *gmioName)
  {
    if (auto ret = xclGMIOWait(DeviceType::get_device_handle(), gmioName))
      throw system_error(ret, "fail to wait gmio");
  }

  virtual int
  start_profiling(int option, const char* port1Name, const char* port2Name, uint32_t value)
  {
    return xclStartProfiling(DeviceType::get_device_handle(), option, port1Name, port2Name, value);
  }

  virtual uint64_t
  read_profiling(int phdl)
  {
    return xclReadProfiling(DeviceType::get_device_handle(), phdl);
  }

  virtual void
  stop_profiling(int phdl)
  {
    if (auto ret = xclStopProfiling(DeviceType::get_device_handle(), phdl))
      throw system_error(ret, "fail to wait gmio");
  }

  virtual void
  load_axlf_meta(const axlf* buffer)
  {
    if (auto ret = xclLoadXclBinMeta(DeviceType::get_device_handle(), buffer))
      throw system_error(ret, "failed to load xclbin");
  }
#endif
};

} // xrt_core

#endif
