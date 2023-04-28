// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_EXT_H_
#define XRT_EXT_H_

// XRT extensions to XRT coreutil
// These extensions are experimental

#include "xrt/detail/config.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_hw_context.h"

#ifdef __cplusplus
# include <cstdint>
#endif

#ifdef __cplusplus
namespace xrt::ext {

///
class bo : public xrt::bo
{
public:

  /**
   * @enum access_mode - buffer object accessibility
   *
   * @var local
   *   Access is local to process and device on which it is allocated
   * @var shared
   *   Access is shared between devices within process
   * @var process
   *   Access is shared between processes and devices
   */
  enum class access_mode : uint8_t { local, shared, process };

  /**
   * bo() - Constructor for buffer object with specific access
   *
   * @param device
   *  The device on which to allocate this buffer
   * @param sz
   *  Size of buffer
   * @param access
   *  Specific access mode for the buffer (see `enum access_mode`)
   *
   * This constructor creates a host_only buffer object with
   * specified access.
   */
  XRT_API_EXPORT
  bo(const xrt::device& device, size_t sz, access_mode access);

  /**
   * bo() - Constructor for buffer object
   *
   * @param device
   *  The device on which to allocate this buffer
   * @param sz
   *  Size of buffer

   * This constructor creates a host_only buffer object with local
   * access.
   */
  XRT_API_EXPORT
  bo(const xrt::device& device, size_t sz);

  /**
   * bo() - Constructor for buffer object
   *
   * @param hwctx
   *  The hardware context that this buffer object uses for queue
   *  operations such as syncing and residency operations.
   * @param sz
   *  Size of buffer
   * @param access
   *  Specific access mode for the buffer (see `enum access_mode`)
   *
   * This constructor creates a host_only buffer object with specified
   * access mode.  The hardware context is used for syncing of data
   * to from device and residency operations, which are all enqueued
   * operations that are synchronized on fence objects.
   */
  XRT_API_EXPORT
  bo(const xrt::hw_context& hwctx, size_t sz, access_mode access);

  /**
   * bo() - Constructor for buffer object
   *
   * @param hwctx
   *  The hardware context that this buffer object uses for queue
   *  operations such as syncing and residency operations.
   * @param sz
   *  Size of buffer
   *
   * This constructor creates a host_only buffer object with local
   * access.
   */
  XRT_API_EXPORT
  bo(const xrt::hw_context& hwctx, size_t sz);

  /**
   * bo() - Constructor to import an exported buffer from another process
   *
   * @param hwctx
   *  The hardware context that this buffer object uses for queue
   *  operations such as syncing and residency operations.
   * @param pid
   *  Process id of exporting process
   * @param ehdl
   *  Exported buffer handle, implementation specific type
   *
   * The exported buffer handle is obtained from exporting process by
   * calling `export_buffer()` on the buffer to be exported.
   */
  XRT_API_EXPORT
  bo(const xrt::hw_context& hwctx, pid_type pid, xclBufferExportHandle ehdl);
};

} // xrt::ext

#else
# error xrt::ext is only implemented for C++
#endif // __cplusplus

#endif
