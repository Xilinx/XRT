// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "core/common/error.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "hip/core/stream.h"

namespace xrt::core::hip {
static void
hipStreamCreateWithFlags(hipStream_t *stream, unsigned int flags)
{
  throw std::runtime_error("Not implemented");
}

static void
hipStreamDestroy(hipStream_t stream)
{
  if (stream == nullptr)
    throw xrt_core::system_error(hipErrorInvalidHandle, "stream is nullptr");

  throw std::runtime_error("Not implemented");
}

static void
hipStreamSynchronize(hipStream_t stream)
{
  throw std::runtime_error("Not implemented");
}

static void
hipStreamWaitEvent(hipStream_t stream, hipEvent_t event, unsigned int flags)
{
  if (event == nullptr) 
    throw xrt_core::system_error(hipErrorInvalidHandle, "event is nullptr");

  throw std::runtime_error("Not implemented");
}
} // // xrt::core::hip

// =========================================================================
// Stream related apis implementation
hipError_t
hipStreamCreateWithFlags(hipStream_t *stream, unsigned int flags)
{
  try {
    xrt::core::hip::hipStreamCreateWithFlags(stream, flags);
    return hipSuccess;
  }
  catch (const xrt_core::system_error& ex) {
    xrt_core::send_exception_message(std::string(__func__) +  " - " + ex.what());
    return static_cast<hipError_t>(ex.value());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

hipError_t
hipStreamDestroy(hipStream_t stream)
{
  try {
    xrt::core::hip::hipStreamDestroy(stream);
    return hipSuccess;
  }
  catch (const xrt_core::system_error& ex) {
    xrt_core::send_exception_message(std::string(__func__) +  " - " + ex.what());
    return static_cast<hipError_t>(ex.value());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

hipError_t
hipStreamSynchronize(hipStream_t stream)
{
  try {
    xrt::core::hip::hipStreamSynchronize(stream);
    return hipSuccess;
  }
  catch (const xrt_core::system_error& ex) {
    xrt_core::send_exception_message(std::string(__func__) +  " - " + ex.what());
    return static_cast<hipError_t>(ex.value());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

hipError_t
hipStreamWaitEvent(hipStream_t stream, hipEvent_t event, unsigned int flags)
{
  try {
    xrt::core::hip::hipStreamWaitEvent(stream, event, flags);
    return hipSuccess;
  }
  catch (const xrt_core::system_error& ex) {
    xrt_core::send_exception_message(std::string(__func__) +  " - " + ex.what());
    return static_cast<hipError_t>(ex.value());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}
