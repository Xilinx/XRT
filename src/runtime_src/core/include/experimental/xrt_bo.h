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
#include "xrt_mem.h"

#ifdef __cplusplus
# include <memory>
#endif

/**
 * typedef xrtDeviceHandle - opaque device handle
 */
typedef void* xrtDeviceHandle;

/**
 * typedef xrtBufferHandle - opaque buffer handle
 */
typedef void* xrtBufferHandle;
  
/**
 * typedef xrtBufferFlags - flags for BO
 *
 * See ``xrt_mem.h`` for available flags
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
   * @param dhdl
   *  Device handle
   * @param userptr
   *  Pointer to aligned user memory
   * @param sz
   *  Size of buffer
   * @param flags
   *  Specify special flags per ``xrt_mem.h``
   * @param grp
   *  Device memory group to allocate buffer in
   */
  XCL_DRIVER_DLLESPEC
  bo(xclDeviceHandle dhdl, void* userptr, size_t sz, buffer_flags flags, memory_group grp);

  
  /**
   * bo() - Constructor with user host buffer 
   *
   * @param dhdl
   *  Device handle
   * @param userptr
   *  Pointer to aligned user memory
   * @param sz
   *  Size of buffer
   * @param grp
   *  Device memory group to allocate buffer in
   *
   * The buffer type is default buffer object with host buffer and
   * device buffer, where the host buffer is managed by user.
   */
  bo(xclDeviceHandle dhdl, void* userptr, size_t sz, memory_group grp)
    : bo(dhdl, userptr, sz, XCL_BO_FLAGS_NONE, grp)
  {}

  /**
   * bo() - Constructor where XRT manages host buffer if any
   *
   * @param dhdl
   *  Device handle
   * @param size
   *  Size of buffer
   * @param flags
   *  Specify special flags per ``xrt_mem.h``
   * @param grp
   *  Device memory group to allocate buffer in
   *
   * If the flags require a host buffer, then the host buffer is allocated by
   * XRT and can be accessed by using map()
   */
  XCL_DRIVER_DLLESPEC
  bo(xclDeviceHandle dhdl, size_t size, buffer_flags flags, memory_group grp);

  /**
   * bo() - Constructor, default flags, where XRT manages host buffer if any
   *
   * @param dhdl
   *  Device handle
   * @param size
   *  Size of buffer
   * @param grp
   *  Device memory group to allocate buffer in
   *
   * The buffer type is default buffer object with host buffer and device buffer.
   * The host buffer is allocated and managed by XRT.
   */
  bo(xclDeviceHandle dhdl, size_t size, memory_group grp)
    : bo(dhdl, size, XCL_BO_FLAGS_NONE, grp)
  {}

  /**
   * bo() - Constructor to import an exported buffer
   *
   * @param dhdl
   *  Device that imports the exported buffer
   * @param ehdl
   *  Exported buffer handle, implementation specific type
   * 
   * The exported buffer handle is acquired by using the export() method
   * and can be passed to another process.  
   */
  XCL_DRIVER_DLLESPEC
  bo(xclDeviceHandle dhdl, xclBufferExportHandle ehdl);

  /**
   * bo() - Constructor for sub-buffer
   *
   * @param parent
   *  Parent buffer
   * @param size
   *  Size of sub-buffer
   * @param offset
   *  Offset into parent buffer
   */
  XCL_DRIVER_DLLESPEC
  bo(const bo& parent, size_t size, size_t offset);

  /**
   * bo() - Copy ctor
   */
  bo(const bo& rhs) = default;

  /**
   * bo() - Move ctor
   */
  bo(bo&& rhs) = default;

  /**
   * operator= () - Move assignment
   */
  bo&
  operator=(bo&& rhs) = default;

  /**
   * size() - Get the size of this buffer
   *
   * @return 
   *  Size of buffer in bytes
   */
  XCL_DRIVER_DLLESPEC
  size_t
  size() const;

  /**
   * address() - Get the device address of this buffer
   *
   * @return 
   *  Device address of buffer
   */
  XCL_DRIVER_DLLESPEC
  uint64_t
  address() const;

  /**
   * buffer_export() - Export this buffer
   *
   * @return 
   *  Exported buffer handle
   *
   * An exported buffer can be imported on another device by this
   * process or another process.
   */
  XCL_DRIVER_DLLESPEC
  xclBufferExportHandle
  export_buffer();

  /**
   * sync() - Synchronize buffer content with device side 
   *
   * @param dir
   *  To device or from device
   * @param sz
   *  Size of data to synchronize
   * @param offset
   *  Offset within the BO
   *
   * Sync specified size bytes of buffer starting at specified offset.
   */
  XCL_DRIVER_DLLESPEC
  void
  sync(xclBOSyncDirection dir, size_t sz, size_t offset);

  /**
   * sync() - Synchronize buffer content with device side 
   *
   * @param dir
   *  To device or from device
   *
   * Sync entire buffer content in specified direction.
   */
  void
  sync(xclBOSyncDirection dir)
  {
    sync(dir, size(), 0);
  }

  /**
   * map() - Map the host side buffer into application
   *
   * @return
   *  Memory mapped buffer
   *
   * Map the contents of the buffer object into host memory
   */
  XCL_DRIVER_DLLESPEC
  void*
  map();

  /**
   * map() - Map the host side buffer info application
   *
   * @tparam MapType
   *  Type of mapped data
   * @return 
   *  Memory mapped buffer
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
   * @param src
   *  Source data pointer
   * @param size
   *  Size of data to copy
   * @param seek
   *  Offset within the BO
   *
   * Copy source data to host buffer of this buffer object.
   * ``seek`` specifies how many bytes to skip at the beginning
   * of the BO before copying-in ``size`` bytes to host buffer.
   */
  XCL_DRIVER_DLLESPEC
  void 
  write(const void* src, size_t size, size_t seek);

  /**
   * write() - Copy-in user data to host backing storage of BO
   *
   * @param src
   *  Source data pointer
   *
   * Copy specified source data to host buffer of this buffer object.
   */
  void
  write(const void* src)
  {
    write(src, size(), 0);
  }

  /**
   * read() - Copy-out user data from host backing storage of BO
   *
   * @param dst
   *  Destination data pointer
   * @param size
   *  Size of data to copy
   * @param skip
   *  Offset within the BO
   *
   * Copy content of host buffer of this buffer object to specified
   * destination.  ``skip`` specifies how many bytes to skip from the
   * beginning of the BO before copying-out ``size`` bytes of host
   * buffer.
   */
  XCL_DRIVER_DLLESPEC
  void
  read(void* dst, size_t size, size_t skip);

  /**
   * read() - Copy-out user data from host backing storage of BO
   *
   * @param dst
   *  Destination data pointer
   *
   * Copy content of host buffer of this buffer object to specified
   * destination.
   */
  void
  read(void* dst)
  {
    read(dst, size(), 0);
  }

  /**
   * copy() - Deep copy BO content from another buffer
   *
   * @param src
   *  Source BO to copy from
   * @param sz
   *  Size of data to copy
   * @param src_offset
   *  Offset into src buffer copy from
   * @param dst_offset
   *  Offset into this buffer to copy to
   *
   * Throws if copy size is 0 or sz + src/dst_offset is out of bounds.
   */
  XCL_DRIVER_DLLESPEC
  void    
  copy(const bo& src, size_t sz, size_t src_offset=0, size_t dst_offset=0);

  /**
   * copy() - Deep copy BO content from another buffer
   *
   * @param src
   *  Source BO to copy from
   *
   * Copy full content of specified src buffer object to this buffer object
   */
  void
  copy(const bo& src)
  {
    copy(src, src.size());
  }

public:
  /// @cond
  std::shared_ptr<bo_impl>
  get_handle() const
  {
    return handle;
  }
  /// @endcond
private:
  std::shared_ptr<bo_impl> handle;
};

} // namespace xrt

/// @cond
extern "C" {
#endif

/**
 * xrtBOAllocUserPtr() - Allocate a BO using userptr provided by the user
 *
 * @dhdl:          Device handle
 * @userptr:       Pointer to 4K aligned user memory
 * @size:          Size of buffer
 * @flags:         Specify type of buffer
 * @grp:           Specify bank information
 * Return:         xrtBufferHandle on success or NULL with errno set
 */
XCL_DRIVER_DLLESPEC
xrtBufferHandle
xrtBOAllocUserPtr(xrtDeviceHandle dhdl, void* userptr, size_t size, xrtBufferFlags flags, xrtMemoryGroup grp);

/**
 * xrtBOAlloc() - Allocate a BO of requested size with appropriate flags
 *
 * @dhdl:          Device handle
 * @size:          Size of buffer
 * @flags:         Specify type of buffer
 * @grp:           Specify bank information
 * Return:         xrtBufferHandle on success or NULL with errno set
 */
XCL_DRIVER_DLLESPEC
xrtBufferHandle
xrtBOAlloc(xrtDeviceHandle dhdl, size_t size, xrtBufferFlags flags, xrtMemoryGroup grp);

/**
 * xrtBOImport() - Allocate a BO imported from another device
 *
 * @dhdl:     Device that imports the exported buffer
 * @ehdl:     Exported buffer handle, implementation specific type
 * 
 * The exported buffer handle is acquired by using the export() method
 * and can be passed to another process.  
 */  
XCL_DRIVER_DLLESPEC
xrtBufferHandle
xrtBOImport(xrtDeviceHandle dhdl, xclBufferExportHandle ehdl);

/**
 * xrtBOExport() - Export this buffer
 *
 * @bhdl:   Buffer handle
 * Return:  Exported buffer handle
 *
 * An exported buffer can be imported on another device by this
 * process or another process.
 */
XCL_DRIVER_DLLESPEC
xclBufferExportHandle
xrtBOExport(xrtBufferHandle bhdl);

/**
 * xrtBOSubAlloc() - Allocate a sub buffer from a parent buffer
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
 * @bhdl:         Buffer handle
 * Return:        0 on success, or err code on error
 */
XCL_DRIVER_DLLESPEC
int
xrtBOFree(xrtBufferHandle bhdl);

/**
 * xrtBOSize() - Get the size of this buffer
 *
 * @bhdl:         Buffer handle
 * Return:        Size of buffer in bytes
 */
XCL_DRIVER_DLLESPEC
size_t
xrtBOSize(xrtBufferHandle bhdl);

/**
 * xrtBOAddr() - Get the physical address of this buffer
 *
 * @bhdl:         Buffer handle
 * Return:        Device address of this BO
 */
XCL_DRIVER_DLLESPEC
uint64_t
xrtBOAddress(xrtBufferHandle bhdl);

/**
 * xrtBOSync() - Synchronize buffer contents in requested direction
 *
 * @bhdl:          Bufferhandle
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
xrtBOSync(xrtBufferHandle bhdl, enum xclBOSyncDirection dir, size_t size, size_t offset);

/**
 * xrtBOMap() - Memory map BO into user's address space
 *
 * @bhdl:       Buffer handle
 * Return:      Memory mapped buffer, or NULL on error with errno set
 *
 * Map the contents of the buffer object into host memory.  The buffer
 * object is unmapped when freed.
 */
XCL_DRIVER_DLLESPEC
void*
xrtBOMap(xrtBufferHandle bhdl);

/**
 * xrtBOWrite() - Copy-in user data to host backing storage of BO
 *
 * @bhdl:          Buffer handle
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
xrtBOWrite(xrtBufferHandle bhdl, const void* src, size_t size, size_t seek);

/**
 * xrtBORead() - Copy-out user data from host backing storage of BO
 *
 * @bhdl:          Buffer handle
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
xrtBORead(xrtBufferHandle bhdl, void* dst, size_t size, size_t skip);

/**
 * xrtBOCopy() - Deep copy BO content from another buffer
 *
 * @dst:          Destination BO to copy to
 * @src:          Source BO to copy from
 * @sz:           Size of data to copy
 * @dst_offset:   Offset into destination buffer to copy to
 * @src_offset:   Offset into src buffer to copy from
 * Return:        0 on success or appropriate error number
 *
 * It is an error if sz is 0 bytes or sz + src/dst_offset is out of bounds.
 */
XCL_DRIVER_DLLESPEC
int
xrtBOCopy(xrtBufferHandle dst, xrtBufferHandle src, size_t sz, size_t dst_offset, size_t src_offset);

/// @endcond  
#ifdef __cplusplus
}
#endif

#endif
