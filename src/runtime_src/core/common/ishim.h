/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include "experimental/xrt-next.h"
#include "experimental/xrt_bo.h"
#include "xrt_graph.h"
#include "error.h"
#include <stdexcept>

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
  export_bo(xclBufferHandle boh) = 0;

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
  exec_buf(xclBufferHandle boh) = 0;

  virtual int
  exec_wait(int timeout_ms) const = 0;

  virtual void
  load_xclbin(const struct axlf*) = 0;

  virtual void
  reclock(const uint16_t* target_freq_mhz) = 0;

#ifdef XRT_ENABLE_AIE
  virtual xclGraphHandle
  open_graph(const xuid_t, const char*) = 0;

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
  sync_aie_bo(xrtBufferHandle bohdl, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset) = 0;

  virtual void
  reset_aie() = 0;

  virtual void
  sync_aie_bo_nb(xrtBufferHandle bohdl, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset) = 0;

  virtual void
  wait_gmio(const char *gmioName) = 0;
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
      throw error(ret, "failed to open ip context");
  }

  virtual void
  close_context(const xuid_t xclbin_uuid, unsigned int ip_index)
  {
    if (auto ret = xclCloseContext(DeviceType::get_device_handle(), xclbin_uuid, ip_index))
      throw error(ret, "failed to close ip context");
  }

  virtual xclBufferHandle
  alloc_bo(size_t size, unsigned int flags)
  {
    if (auto bo = xclAllocBO(DeviceType::get_device_handle(), size, 0, flags))
      return bo;
    throw std::bad_alloc();
  }

  virtual xclBufferHandle
  alloc_bo(void* userptr, size_t size, unsigned int flags)
  {
    if (auto bo = xclAllocUserPtrBO(DeviceType::get_device_handle(), userptr, size, flags))
      return bo;
    throw std::bad_alloc();
  }

  virtual void
  free_bo(xclBufferHandle bo)
  {
    xclFreeBO(DeviceType::get_device_handle(), bo);
  }

  virtual xclBufferExportHandle
  export_bo(xclBufferHandle bo)
  {
    auto ehdl = xclExportBO(DeviceType::get_device_handle(), bo);
    if (ehdl == XRT_NULL_BO_EXPORT)
      throw std::runtime_error("Unable to export BO");
    return ehdl;
  }

  virtual xclBufferHandle
  import_bo(xclBufferExportHandle ehdl)
  {
    if (auto bo = xclImportBO(DeviceType::get_device_handle(), ehdl, 0))
      return bo;
    throw std::runtime_error("unable to import BO");
  }

  virtual void
  copy_bo(xclBufferHandle dst, xclBufferHandle src, size_t size, size_t dst_offset, size_t src_offset)
  {
    if (auto err = xclCopyBO(DeviceType::get_device_handle(), dst, src, size, dst_offset, src_offset))
      throw std::runtime_error("unable to copy BO");
  }

  virtual void
  sync_bo(xclBufferHandle bo, xclBOSyncDirection dir, size_t size, size_t offset)
  {
    if (auto err = xclSyncBO(DeviceType::get_device_handle(), bo, dir, size, offset))
      throw std::runtime_error("unable to sync BO");
  }

  virtual void*
  map_bo(xclBufferHandle bo, bool write)
  {
    if (auto mapped = xclMapBO(DeviceType::get_device_handle(), bo, write))
      return mapped;
    throw std::runtime_error("could not map BO");
  }

  virtual void
  unmap_bo(xclBufferHandle bo, void* addr)
  {
    if (auto ret = xclUnmapBO(DeviceType::get_device_handle(), bo, addr))
      throw error(ret, "failed to unmap BO");
  }

  virtual void
  get_bo_properties(xclBufferHandle bo, struct xclBOProperties *properties) const
  {
    if (auto ret = xclGetBOProperties(DeviceType::get_device_handle(), bo, properties))
      throw error(ret, "failed to get BO properties");
  }

  virtual void
  reg_read(uint32_t ipidx, uint32_t offset, uint32_t* data) const
  {
    if (auto ret = xclRegRead(DeviceType::get_device_handle(), ipidx, offset, data))
      throw error(ret, "failed to read ip(" + std::to_string(ipidx) + ")");
  }

  virtual void
  reg_write(uint32_t ipidx, uint32_t offset, uint32_t data)
  {
    if (auto ret = xclRegWrite(DeviceType::get_device_handle(), ipidx, offset, data))
      throw error(ret, "failed to write ip(" + std::to_string(ipidx) + ")");
  }

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  virtual void
  xread(uint64_t offset, void* buffer, size_t size) const
  {
    if (size != xclRead(DeviceType::get_device_handle(), XCL_ADDR_KERNEL_CTRL, offset, buffer, size))
      throw error(1, "failed to read at address (" + std::to_string(offset) + ")");
  }

  virtual void
  xwrite(uint64_t offset, const void* buffer, size_t size)
  {
    if (size != xclWrite(DeviceType::get_device_handle(), XCL_ADDR_KERNEL_CTRL, offset, buffer, size))
      throw error(1, "failed to write at address (" + std::to_string(offset) + ")");
  }
#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif


  virtual void
  exec_buf(xclBufferHandle bo)
  {
    if (auto ret = xclExecBuf(DeviceType::get_device_handle(), bo))
      throw error(ret, "failed to launch execution buffer");
  }

  virtual int
  exec_wait(int timeout_ms) const
  {
    return xclExecWait(DeviceType::get_device_handle(), timeout_ms);
  }

  virtual void
  load_xclbin(const struct axlf* buffer)
  {
    if (auto ret = xclLoadXclBin(DeviceType::get_device_handle(), buffer))
      throw error(ret, "failed to load xclbin");
  }

  virtual void
  reclock(const uint16_t* target_freq_mhz)
  {
    if (auto ret = xclReClock2(DeviceType::get_device_handle(), 0, target_freq_mhz))
      throw error(ret, "failed to reclock specified clock");
  }

#ifdef XRT_ENABLE_AIE
  virtual xclGraphHandle
  open_graph(const xuid_t uuid, const char *gname)
  {
    if (auto ghdl = xclGraphOpen(DeviceType::get_device_handle(), uuid, gname))
      return ghdl;

    throw std::runtime_error("failed to open graph");
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
      throw error(ret, "fail to reset graph");
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
      throw error(ret, "fail to run graph");
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
      throw error(ret, "fail to wait graph");
  }

  virtual void
  suspend_graph(xclGraphHandle handle)
  {
    if (auto ret = xclGraphSuspend(handle))
      throw error(ret, "fail to suspend graph");
  }

  virtual void
  resume_graph(xclGraphHandle handle)
  {
    if (auto ret = xclGraphResume(handle))
      throw error(ret, "fail to resume graph");
  }

  virtual void
  end_graph(xclGraphHandle handle, uint64_t cycle)
  {
    if (auto ret = xclGraphEnd(handle, cycle))
      throw error(ret, "fail to end graph");
  }

  virtual void
  update_graph_rtp(xclGraphHandle handle, const char* port, const char* buffer, size_t size)
  {
    if (auto ret = xclGraphUpdateRTP(handle, port, buffer, size))
      throw error(ret, "fail to update graph rtp");
  }

  virtual void
  read_graph_rtp(xclGraphHandle handle, const char* port, char* buffer, size_t size)
  {
    if (auto ret = xclGraphReadRTP(handle, port, buffer, size))
      throw error(ret, "fail to read graph rtp");
  }

  virtual void
  sync_aie_bo(xrtBufferHandle bohdl, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset)
  {
    if (auto ret = xclSyncBOAIE(DeviceType::get_device_handle(), bohdl, gmioName, dir, size, offset))
      throw error(ret, "fail to sync aie bo");
  }

  virtual void
  reset_aie()
  {
    if (auto ret = xclResetAIEArray(DeviceType::get_device_handle()))
      throw error(ret, "fail to reset aie");
  }

  virtual void
  sync_aie_bo_nb(xrtBufferHandle bohdl, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset)
  {
    if (auto ret = xclSyncBOAIENB(DeviceType::get_device_handle(), bohdl, gmioName, dir, size, offset))
      throw error(ret, "fail to sync aie non-blocking bo");
  }

  virtual void
  wait_gmio(const char *gmioName)
  {
    if (auto ret = xclGMIOWait(DeviceType::get_device_handle(), gmioName))
      throw error(ret, "fail to wait gmio");
  }
#endif
};

} // xrt_core

#endif
