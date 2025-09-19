// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrthip_error_h
#define xrthip_error_h

#include "core/common/device.h"
#include "core/common/api/bo.h"

#include "hip/config.h"
#include "hip/core/device.h"
#include "hip/hip_runtime_api.h"

#include "xrt/experimental/xrt_error.h"

namespace xrt::core::hip
{
/**
 * @class error
 * @brief Handles HIP error management, including last error tracking and thread-local error strings.
 */
class error
{
public:
  /**
   * @brief Get the singleton instance of error manager.
   * @return Reference to the error instance.
   */
  static
  error&
  instance();

  /**
   * @brief Get error name string for a given HIP error code.
   * @param err HIP error code.
   * @return Error name string.
   */
  static
  const char*
  get_error_name(hipError_t err);

  /**
   * @brief Record a thread-local error string for a given HIP error code.
   * @param err HIP error code.
   * @param err_str Error string.
   */
  void
  record_local_error(hipError_t err, const std::string& err_str);

  /**
   * @brief Reset all thread-local error strings.
   */
  void
  reset_local_errors();

  /**
   * @brief Get the thread-local error string for a given HIP error code.
   * @param err HIP error code.
   * @return Error string.
   */
  const char*
  get_local_error_string(hipError_t err);

  /**
   * @brief get async error from XRT and update @m_last_error
   * @return HIP error code if there was last async error, otherwise
   *         return hipSuccess
   */
  hipError_t
  get_last_error();

  /**
   * @brief peek async error which returns the last async error
   *        without updating it. That is if you call this function
   *        multiple times, and no get_last_error() is called between,
   *        it will always return the same error.
   * @return HIP error code if there was last async error, otherwise
   *         return hipSuccess
   */
  hipError_t
  peek_last_error();

private:
  /**
   * @brief Protected constructor for singleton pattern.
   */
  error();

  /**
   * @brief Thread-local map of HIP error codes to error strings.
   */
  std::map<hipError_t, std::string> m_local_errors;

  /**
   * @brief update the last async error stored in HIP
   * @return hip error code if there there is new error got from XRT, hipSuccess otherwise.
   */
  hipError_t
  update_last_error();

  /**
   * @brief get error string for the last async error
   * This function is called to get the last async error string
   * @return Error string for the last async error.
   */
  std::string
  get_last_error_string();

  /**
   * @struct async_error_info
   * @brief async error information for a device
   */
  struct async_error_info
  {
    std::string err_str; // error string
    xrtErrorTime timestamp; // timestamp for the last error
  };

  hipError_t m_hip_last_error; // last async HIP error
  std::map<device_handle, async_error_info> m_xrt_last_errors; // last AIE async xrt error per device
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
