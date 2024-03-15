// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_error_h
#define xrthip_error_h

#include "xrt/config.h"
#include "xrt/device/hal.h"
#include "xrt/util/range.h"
#include "core/common/device.h"
#include "core/common/api/bo.h"

namespace xrt::core::hip
{
  class error_state
  {
  public:
    static error_state& instance();

    static const char* get_error_name(hipError_t err);

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
    error_state();

  private:
    hipError_t m_last_error;
  }; // class error_state
}
#endif // xrthip_error_h