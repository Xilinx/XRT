// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.

#ifdef _WIN32
#pragma warning(disable : 4201)
#pragma warning(disable : 4100)
#endif

#include <string>

#include "hip/config.h"
#include "hip/hip_runtime_api.h"
#include "hip/core/device.h"
#include "core/common/error.h"
#include "hip/core/error_state.h"

namespace xrt::core::hip
{

  static hipError_t
  hip_peek_last_error()
  {
    hipError_t last_error = xrt::core::hip::error_state::GetInstance()->peek_last_error();
    return last_error;
  }

  static hipError_t
  hip_get_last_error()
  {
    hipError_t last_error = xrt::core::hip::error_state::GetInstance()->peek_last_error();
    xrt::core::hip::error_state::GetInstance()->reset_last_error();
    return last_error;
  }

} // xrt::core::hip

// Return hip error as text string form.
hipError_t
hipDrvGetErrorName(hipError_t hipError,
                   const char **errorName)
{
  try {
    *errorName = xrt::core::hip::error_state::get_error_name(hipError);
    return hipSuccess;
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorInvalidValue;
}

// Return handy text string message to explain the error which occurred.
hipError_t
hipDrvGetErrorString(hipError_t hipError,
                     const char **errorString)
{
  try {
    // TODO: return more detailed erro string instead of error name
    *errorString = xrt::core::hip::error_state::get_error_name(hipError);
    return hipSuccess;
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorInvalidValue;
}

// return last error returned by any HIP API call and resets the stored error code
hipError_t
hipExtGetLastError()
{
  hipError_t last_error = hipSuccess;
  try {
    last_error = xrt::core::hip::hip_get_last_error();
    return last_error;
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return last_error;
}

// Return handy text string message to explain the error which occurred.
const char *
hipGetErrorString(hipError_t hipError)
{
  const char *error_string = nullptr;
  try {
    // TODO: return more detailed erro string instead of error name
    error_string = xrt::core::hip::error_state::get_error_name(hipError);
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return error_string;
}

// Return hip error as text string form.
const char *
hipGetErrorName(hipError_t hipError)
{
  const char *error_name = nullptr;
  try {
    error_name = xrt::core::hip::error_state::get_error_name(hipError);
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return error_name;
}

// return last error returned by any HIP API call and resets the stored error code
hipError_t
hipGetLastError(void)
{
  hipError_t last_error = hipSuccess;
  try {
    last_error = xrt::core::hip::hip_get_last_error();
    return last_error;
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return last_error;
}

// Return last error returned by any HIP runtime API call.
hipError_t hipPeekAtLastError()
{
  hipError_t last_error = hipSuccess;
  try {
    last_error = xrt::core::hip::hip_peek_last_error();
    return last_error;
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return last_error;
}