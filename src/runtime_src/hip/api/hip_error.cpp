// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/error.h"
#include "hip/config.h"
#include "hip/hip_runtime_api.h"
#include "hip/core/common.h"
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
    *errorString = xrt::core::hip::error::instance().get_local_error_string(hipError);
    return hipSuccess;
  }
  catch (std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (...) {
    // Does nothing
  }

  return hipErrorRuntimeOther;
}

// Return handy text string message to explain the error which occurred.
const char *
hipGetErrorString(hipError_t hipError)
{
  try {
    return xrt::core::hip::error::instance().get_local_error_string(hipError);
  }
  catch(...) {
    // Does nothing
  }
  return nullptr;
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
    hipError_t last_err = hipSuccess;
    auto ret = handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
      last_err = xrt::core::hip::hip_get_last_error();
    });

    if (ret == hipSuccess)
      ret = last_err;

    return ret;
}

// Return last error returned by any HIP runtime API call.
hipError_t hipPeekAtLastError()
{
    hipError_t last_err = hipSuccess;
    auto ret = handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
      last_err = xrt::core::hip::hip_peek_last_error();
    });

    if (ret == hipSuccess)
      ret = last_err;

    return ret;
}
