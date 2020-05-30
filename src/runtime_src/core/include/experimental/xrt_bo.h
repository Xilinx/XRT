/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

#ifndef _XRT_BO_H_
#define _XRT_BO_H_

#include "xrt.h"

#ifdef __cplusplus
# include <memory>
#endif

/**
 * typedef xrtDeviceHandle - opaque device handle
 *
 * Typedef alias from xrt.h
 */
typedef xclDeviceHandle xrtDeviceHandle;

/**
 * typedef xrtBufferHandle - opaque buffer handle
 */
typedef void* xrtBufferHandle;
  
/**
 * typedef xrtBufferFlags - flags for BO
 */
typedef uint64_t xrtBufferFlags;

/**
 * typedef xrtMemoryGroup - Memory bank group for buffer
 */
typedef uint32_t xrtMemoryGroup;
  
#ifdef __cplusplus

namespace xrt {

using buffer_flags = xrtBufferFlags;  
using memory_group = xrtMemoryGroup;

class bo_impl;
class bo
{
public:
  /**
   * bo() - Constructor for empty bo
   */
  bo()
  {}

  /**
   * bo() - Constructor with user host buffer and flags
   *
   * @dhdl:     Device handle
   * @userptr:  Pointer to aligned user memory
   * @size:     Size of buffer
   * @flags:    Specify special flags per ``xrt_mem.h``
   * @grp:      Device memory group to allocate buffer in
   */
  XCL_DRIVER_DLLESPEC
  bo(xclDeviceHandle dhld, void* userptr, size_t sz, buffer_flags flags, memory_group grp);

  /**
   * bo() - Constructor where XRT manages host buffer if any
   *
   * @dhdl:     Device handle
   * @size:     Size of buffer
   * @flags:    Specify special flags per ``xrt_mem.h``
   * @grp:      Device memory group to allocate buffer in
   *
   * If the flags require a host buffer, then the host buffer is allocated by
   * XRT and can be accessed by using @map()
   */
  XCL_DRIVER_DLLESPEC
  bo(xclDeviceHandle dhdl, size_t size, buffer_flags flags, memory_group grp);

  /**
   * bo() - Constructor for sub-buffer
   *
   * @parent:   Parent buffer
   * @size:     Size of sub-buffer
   * @size:     Offset into parent buffer
   */
  XCL_DRIVER_DLLESPEC
  bo(const bo& parent, size_t size, size_t offset);

  /**
   * bo() - Copy ctor
   */
  bo(const bo& rhs)
    : handle(rhs.handle)
  {}

  /**
   * bo() - Move ctor
   */
  bo(bo&& rhs)
    : handle(std::move(rhs.handle))
  {}

  /**
   * operator= () - Move assignment
   */
  bo&
  operator=(bo&& rhs)
  {
    handle = std::move(rhs.handle);
    return *this;
  }

  /**
   * sync() - Synchronize buffer content with device side 
   *
   * @dir:     To device or from device
   * @size:    Size of data to synchronize
   * @offset:  Offset within the BO
   */
  XCL_DRIVER_DLLESPEC
  void
  sync(xclBOSyncDirection dir, size_t size, size_t offset);

  /**
   * map() - Map the host side buffer into application
   *
   * Return: Memory mapped buffer
   *
   * Map the contents of the buffer object into host memory
   */
  XCL_DRIVER_DLLESPEC
  void*
  map();

  /**
   * map() - Map the host side buffer info application
   *
   * @MapType: Type of mapped data
   * Return: Memory mapped buffer
   */
  template<typename MapType>
  MapType
  map()
  {
    return reinterpret_cast<MapType>(map());
  }

  /**
   * write() - Copy-in user data to host backing storage of BO
   *
   * @src:   Source data pointer
   * @size:  Size of data to copy
   * @seek:  Offset within the BO
   *
   * Copy host buffer contents to previously allocated device
   * memory. ``seek`` specifies how many bytes to skip at the beginning
   * of the BO before copying-in ``size`` bytes of host buffer.
   */
  XCL_DRIVER_DLLESPEC
  void 
  write(const void* src, size_t size, size_t seek);

  /**
   * read() - Copy-out user data from host backing storage of BO
   *
   * @dst:           Destination data pointer
   * @size:          Size of data to copy
   * @skip:          Offset within the BO
   *
   * Copy contents of previously allocated device memory to host
   * buffer. ``skip`` specifies how many bytes to skip from the
   * beginning of the BO before copying-out ``size`` bytes of device
   * buffer.
   */
  XCL_DRIVER_DLLESPEC
  void
  read(void* dst, size_t size, size_t skip);

public:
  std::shared_ptr<bo_impl>
  get_handle() const
  {
    return handle;
  }

private:
  std::shared_ptr<bo_impl> handle;
};

} // namespace xrt

extern "C" {
#endif

/**
 * xrtBOAllocUserPtr() - Allocate a BO using userptr provided by the user
 *
 * @handle:        Device handle
 * @userptr:       Pointer to 4K aligned user memory
 * @size:          Size of buffer
 * @flags:         Specify bank information, etc
 * Return:         xrtBufferHandle on success or NULL with errno set
 */
XCL_DRIVER_DLLESPEC
xrtBufferHandle
xrtBOAllocUserPtr(xclDeviceHandle dhdl, void* userptr, size_t size, xrtBufferFlags flags, xrtMemoryGroup grp);

/**
 * xclBOAlloc() - Allocate a BO of requested size with appropriate flags
 *
 * @handle:        Device handle
 * @size:          Size of buffer
 * @unused:        This argument is ignored
 * @flags:         Specify bank information, etc
 * Return:         xrtBufferHandle on success or NULL with errno set
 */
XCL_DRIVER_DLLESPEC
xrtBufferHandle
xrtBOAlloc(xclDeviceHandle dhdl, size_t size, xrtBufferFlags flags, xrtMemoryGroup grp);

/**
 * xclBOSubAlloc() - Allocate a sub buffer from a parent buffer
 *
 * @parent:        Parent buffer handle
 * @size:          Size of sub buffer 
 * @offset:        Offset into parent buffer
 * Return:         xrtBufferHandle on success or NULL with errno set
 */
XCL_DRIVER_DLLESPEC
xrtBufferHandle
xrtBOSubAlloc(xrtBufferHandle parent, size_t size, size_t offset);

/**
 * xrtBOFree() - Free a previously allocated BO
 *
 * @handle:       Buffer handle
 * Return:        0 on success, or err code on error
 */
XCL_DRIVER_DLLESPEC
int
xrtBOFree(xrtBufferHandle handle);

/**
 * xrtBOSync() - Synchronize buffer contents in requested direction
 *
 * @handle:        Bufferhandle
 * @dir:           To device or from device
 * @size:          Size of data to synchronize
 * @offset:        Offset within the BO
 * Return:         0 on success or standard errno
 *
 * Synchronize the buffer contents between host and device. Depending
 * on the memory model this may require DMA to/from device or CPU
 * cache flushing/invalidation
 */
XCL_DRIVER_DLLESPEC
int
xrtBOSync(xrtBufferHandle handle, xclBOSyncDirection dir, size_t size, size_t offset);

/**
 * xrtBOMap() - Memory map BO into user's address space
 *
 * @handle:     Buffer handle
 * Return:      Memory mapped buffer, or NULL on error with errno set
 *
 * Map the contents of the buffer object into host memory
 * To unmap the buffer call xclUnmapBO().
 */
XCL_DRIVER_DLLESPEC
void*
xrtBOMap(xrtBufferHandle handle);

/**
 * xrtBOWrite() - Copy-in user data to host backing storage of BO
 *
 * @handle:        Buffer handle
 * @src:           Source data pointer
 * @size:          Size of data to copy
 * @seek:          Offset within the BO
 * Return:         0 on success or appropriate error number
 *
 * Copy host buffer contents to previously allocated device
 * memory. ``seek`` specifies how many bytes to skip at the beginning
 * of the BO before copying-in ``size`` bytes of host buffer.
 */
XCL_DRIVER_DLLESPEC
int
xrtBOWrite(xrtBufferHandle handle, const void* src, size_t size, size_t seek);

/**
 * xrtBORead() - Copy-out user data from host backing storage of BO
 *
 * @handle:        Buffer handle
 * @dst:           Destination data pointer
 * @size:          Size of data to copy
 * @skip:          Offset within the BO
 * Return:         0 on success or appropriate error number
 *
 * Copy contents of previously allocated device memory to host
 * buffer. ``skip`` specifies how many bytes to skip from the
 * beginning of the BO before copying-out ``size`` bytes of device
 * buffer.
 */
XCL_DRIVER_DLLESPEC
int
xrtBORead(xrtBufferHandle handle, void* dst, size_t size, size_t skip);

#ifdef __cplusplus
}
#endif

#endif
