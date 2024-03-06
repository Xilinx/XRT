// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_error_state_h
#define xrthip_error_state_h

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
    ~error_state() 
    {
    }

    static error_state* GetInstance();

    static const char* get_error_name(hipError_t err);

    hipError_t peek_last_error()
    {
      return m_last_error;
    }

    void reset_last_error()
    {
      m_last_error = hipSuccess;
    }

    void set_last_error(hipError_t err)
    {
      m_last_error = err;
    }

  protected:
    error_state()
    {
    }

    static error_state *m_error_state;

  private:
    thread_local static hipError_t m_last_error;
  }; // class error_state

}
#endif // xrthip_error_state_h