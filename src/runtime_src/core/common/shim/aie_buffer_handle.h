// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef XRT_CORE_AIE_BUFFER_HANDLE_H
#define XRT_CORE_AIE_BUFFER_HANDLE_H
#include <vector>
#include "xrt.h"
#include "xrt/xrt_bo.h"
namespace xrt_core {
class aie_buffer_handle
{
public:
  virtual ~aie_buffer_handle() {}

  virtual std::string
  get_name() const
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  virtual void
  sync(std::vector<xrt::bo>&, xclBOSyncDirection, size_t, size_t) const
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  virtual void
  async(std::vector<xrt::bo>&, xclBOSyncDirection, size_t, size_t)
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  virtual void
  wait()
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

};

} // xrt_core
#endif
