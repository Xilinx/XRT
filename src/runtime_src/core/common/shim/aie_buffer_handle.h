// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XRT_CORE_AIE_BUFFER_HANDLE_H
#define XRT_CORE_AIE_BUFFER_HANDLE_H
#include <vector>
#include <system_error>
#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "core/common/error.h"
#include "xrt/xrt_aie.h"

namespace xrt_core {

// class aie_buffer_handle - shim base class for aie buffer objects
//
// shim level implementation derives off this class to support aie buffer objects
class aie_buffer_handle
{
public:
  // Destruction must destroy the underlying aie buffer object
  virtual ~aie_buffer_handle() {}

  // Get the port name of this aie buffer object
  virtual std::string
  get_name() const
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  // Sync a buffer from AIE_to_GMIO or GMIO_to_AIE
  virtual void
  sync(std::vector<xrt::bo>&, xclBOSyncDirection, size_t, size_t) const
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  // Async a buffer from AIE_to_GMIO or GMIO_to_AIE
  virtual void
  async(std::vector<xrt::bo>&, xclBOSyncDirection, size_t, size_t)
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  // Get the state of this aie buffer object
  virtual xrt::aie::device::buffer_state
  async_status()
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  // Wait for the async execution to complete
  virtual void
  wait()
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

};

} // xrt_core
#endif
