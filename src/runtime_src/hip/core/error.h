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

/**
 * @class hip_exception
 * @brief Exception class for HIP errors.
 */
class hip_exception: public std::exception
{
public:
  /**
   * @brief Construct a HIP exception.
   * @param ec HIP error code.
   * @param what Error description string.
   */
  hip_exception(hipError_t ec, const char* what);

  /**
   * @brief Get the HIP error code value.
   * @return HIP error code.
   */
  hipError_t
  value() const noexcept;

  /**
   * @brief Get the error description string.
   * @return Error description.
   */
  const char*
  what() const noexcept override;
private:
  hipError_t m_code; // HIP error code
  std::string m_what; // Error description string
};

/**
 * @brief Convert a system error code to a HIP error code.
 * @param serror System error code.
 * @return Corresponding HIP error code.
 */
hipError_t
system_to_hip_error(int serror);

}
#endif // xrthip_error_h
