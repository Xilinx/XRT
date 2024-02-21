// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "core/common/error.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "hip/core/event.h"

namespace xrt::core::hip {

void throw_if_null(hipEvent_t event)
{
  if (!event)
    throw xrt_core::system_error(hipErrorInvalidHandle, "event is nullptr");
}
static hipEvent_t
hip_event_create()
{
  throw std::runtime_error("Not implemented");
}

static void
hip_event_destroy(hipEvent_t event)
{
  throw_if_null(event);
  throw std::runtime_error("Not implemented");
}

static void
hip_event_record(hipEvent_t event, hipStream_t stream)
{
  throw_if_null(event);
  throw std::runtime_error("Not implemented");
}

static void
hip_event_synchronize(hipEvent_t event)
{
 throw_if_null(event);
 throw std::runtime_error("Not implemented");
}

static float
hip_event_elapsed_time(hipEvent_t start, hipEvent_t stop)
{
  throw_if_null(stop);
  throw_if_null(start);
  throw std::runtime_error("Not implemented");
}

static unsigned short
hip_event_query(hipEvent_t event)
{
  throw_if_null(event);
  throw std::runtime_error("Not implemented");
}
} // // xrt::core::hip

// =========================================================================
//                    Event APIs implementation
// =========================================================================
hipError_t
hipEventCreate(hipEvent_t* event)
{
  try {
    if (!event)
      throw xrt_core::system_error(hipErrorInvalidValue, "event passed is nullptr");

    *event = xrt::core::hip::hip_event_create();
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
hipEventDestroy(hipEvent_t event)
{
  try {
    xrt::core::hip::hip_event_destroy(event);
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
hipEventSynchronize(hipEvent_t event)
{
  try {
    xrt::core::hip::hip_event_synchronize(event);
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
hipEventRecord(hipEvent_t event, hipStream_t stream)
{
  try {
    xrt::core::hip::hip_event_record(event, stream);
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
hipEventQuery (hipEvent_t event)
{
  try {
    xrt::core::hip::hip_event_query(event);
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
hipEventElapsedTime (float *ms, hipEvent_t start, hipEvent_t stop)
{
  try {
    if (!ms)
      throw xrt_core::system_error(hipErrorInvalidValue, "the ms (elapsed time output) passed is nullptr");

    *ms = xrt::core::hip::hip_event_elapsed_time(start, stop);
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
