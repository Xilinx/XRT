// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_CORE_PROFILE_HANDLE_H
#define XRT_CORE_PROFILE_HANDLE_H

#include <stdint.h>
#include "core/common/error.h"

namespace xrt_core {

class profile_handle
{
public:
  virtual int
  start(int, const char*, const char*, uint32_t)
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  virtual uint64_t
  read()
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }

  virtual void
  stop()
  {
    throw xrt_core::error(std::errc::not_supported, __func__);
  }
  
}; //profile_handle 

} //namespace xrt_core
#endif
