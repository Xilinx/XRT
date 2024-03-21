// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "core/common/error.h"
#include "hip/config.h"
#include "hip/hip_runtime_api.h"
#include "hip/core/device.h"
#include "hip/core/error.h"
#include <string>

namespace xrt::core::hip
{
  static hipError_t
  hip_peek_last_error()
  {
    return error::instance().peek_last_error();
  }

  static hipError_t
  hip_get_last_error()
  {
    hipError_t last_error = error::instance().peek_last_error();
    error::instance().reset_last_error();
    return last_error;
  }

} // xrt::core::hip

// Return hip error as text string form.
hipError_t
hipDrvGetErrorName(hipError_t hipError,
                   const char **errorName)
{
  try {
    *errorName = xrt::core::hip::error::get_error_name(hipError);
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
    // TODO: return more detailed error string instead of error name
    *errorString = xrt::core::hip::error::get_error_name(hipError);
    return hipSuccess;
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorInvalidValue;
}

// Return handy text string message to explain the error which occurred.
const char *
hipGetErrorString(hipError_t hipError)
{
  const char *error_string = nullptr;
  try {
    // TODO: return more detailed error string instead of error name
    error_string = xrt::core::hip::error::get_error_name(hipError);
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
    error_name = xrt::core::hip::error::get_error_name(hipError);
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return error_name;
}

template<typename F> hipError_t
handle_hip_error_error(F && f)
{
  hipError_t last_error = hipSuccess;
  try {
    return f();
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return last_error;
}

// return last error returned by any HIP API call and resets the stored error code
hipError_t
hipExtGetLastError()
{
  return hipGetLastError();
}

// return last error returned by any HIP API call and resets the stored error code
hipError_t
hipGetLastError(void)
{
  return handle_hip_error_error([&] { return xrt::core::hip::hip_get_last_error(); });
}

// Return last error returned by any HIP runtime API call.
hipError_t hipPeekAtLastError()
{
  return handle_hip_error_error([&] { return xrt::core::hip::hip_peek_last_error(); });
}

//hipError_t handle_hip_error([&] { xrt::core::hip::hipMemCpy(dst, src, sizeBytes, kind); });
