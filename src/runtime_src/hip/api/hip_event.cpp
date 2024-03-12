// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "core/common/error.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "hip/core/event.h"
#include "hip/core/stream.h"

namespace xrt::core::hip {

static command_handle
hip_event_create()
{
  return insert_in_map(command_cache, std::make_shared<event>(nullptr));;
}

static void
hip_event_destroy(hipEvent_t event)
{
  throw_invalid_value_if(!event, "event passed is nullptr");
  command_cache.remove(event);
}

static void
hip_event_record(hipEvent_t event, hipStream_t stream)
{
  throw_invalid_value_if(!event, "event passed is nullptr");
  throw_invalid_value_if(!stream, "stream passed is nullptr");
  //auto hip_stream = get_stream(stream);It is commented will be uncomment after stream PR is merged
  auto hip_event_cmd = command_cache.get(event);
  //hip_event_cmd->record(hip_stream);It is commented will be uncomment after stream PR is merged
}

static void
hip_event_synchronize(hipEvent_t event)
{
 throw_invalid_value_if(!event, "event passed is nullptr");
 auto hip_event_cmd = command_cache.get(event);
 hip_event_cmd->synchronize();
}

static float
hip_event_elapsed_time(hipEvent_t start, hipEvent_t stop)
{
  throw_invalid_value_if(!start, "start event passed is nullptr");
  throw_invalid_value_if(!stop, "stop event passed is nullptr");
  auto hip_start_event_cmd = command_cache.get(start);
  auto hip_stop_event_cmd = command_cache.get(stop);
  return hip_start_event_cmd->elapsedtimecalc(hip_stop_event_cmd);
}

static unsigned short
hip_event_query(hipEvent_t event)
{
  throw_invalid_value_if(!event, "event passed is nullptr");
  auto hip_event_cmd = command_cache.get(event);
  if(hip_event_cmd->query()){
    return hipSuccess;
  }
  else{
    return hipErrorNotReady;
  }
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

    auto handle = xrt::core::hip::hip_event_create();
    *event = reinterpret_cast<hipEvent_t>(handle);
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
    if(!event)
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

