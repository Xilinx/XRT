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
  open_context(xuid_t xclbin , unsigned int ip_index, bool shared) = 0;

  virtual void
  close_context(xuid_t xclbin, unsigned int ip_index) = 0;

  virtual xclBufferHandle
  alloc_bo(size_t size, unsigned int flags) = 0;

  virtual void
  free_bo(xclBufferHandle bohdl) = 0;

  virtual void*
  map_bo(xclBufferHandle boHandle, bool write) = 0;

  virtual void
  unmap_bo(xclBufferHandle boHandle, void* addr) = 0;

  virtual void
  get_bo_properties(xclBufferHandle boHandle, struct xclBOProperties *properties) = 0;
};

template <typename DeviceType>
struct shim : public DeviceType
{
  template <typename ...Args>
  shim(Args&&... args)
    : DeviceType(std::forward<Args>(args)...)
  {}

  virtual void
  open_context(xuid_t xclbin , unsigned int ip_index, bool shared)
  {
    if (auto ret = xclOpenContext(DeviceType::get_device_handle(), xclbin, ip_index, shared))
      throw error(ret, "failed to open ip context");
  }

  virtual void
  close_context(xuid_t xclbin, unsigned int ip_index)
  {
    if (auto ret = xclCloseContext(DeviceType::get_device_handle(), xclbin, ip_index))
      throw error(ret, "failed to close ip context");
  }

  virtual xclBufferHandle
  alloc_bo(size_t size, unsigned int flags)
  {
    if (auto bo = xclAllocBO(DeviceType::get_device_handle(), size, 0, flags))
      return bo;
    throw std::bad_alloc();
  }

  virtual void
  free_bo(xclBufferHandle bo)
  {
    xclFreeBO(DeviceType::get_device_handle(), bo);
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
  get_bo_properties(xclBufferHandle bo, struct xclBOProperties *properties)
  {
    if (auto ret = xclGetBOProperties(DeviceType::get_device_handle(), bo, properties))
      throw error(ret, "failed to get BO properties");
  }
};

} // xrt_core

#endif
