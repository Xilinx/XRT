// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrthip_error_h
#define xrthip_error_h

#include "core/common/device.h"
#include "core/common/api/bo.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

namespace xrt::core::hip
{
  class error
  {
  public:
    static error&
    instance();

    static const char*
    get_error_name(hipError_t err);

    hipError_t
    peek_last_error()
    {
      return m_last_error;
    }

    void
    reset_last_error()
    {
      m_last_error = hipSuccess;
    }

    void
    set_last_error(hipError_t err)
    {
      m_last_error = err;
    }

  protected:
    error();

  private:
    hipError_t m_last_error;
  }; // class error
}
#endif // xrthip_error_h
