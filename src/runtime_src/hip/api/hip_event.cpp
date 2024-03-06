// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "core/common/error.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "hip/core/event.h"
#include "hip/core/stream.h"

namespace xrt::core::hip {

static command_handle hip_event_create()
{
  // Event when created doesn't have any stream associated with it
  // It is pushed into stream when recorded
  return insert_in_map(command_cache, std::make_shared<event>());
}

static void hip_event_destroy(hipEvent_t eve)
{
  throw_invalid_value_if(!eve, "event passed is nullptr");
  command_cache.remove(eve);
}

static void hip_event_record(hipEvent_t eve, hipStream_t stream)
{
  throw_invalid_value_if(!eve, "event passed is nullptr");
  throw_invalid_value_if(!stream, "stream passed is nullptr");
  /* TODO
  auto hip_stream = get_stream(stream);
  auto hip_ev = std::dynamic_pointer_cast<event>(command_cache.get(eve));
  hip_ev->record(hip_stream);
  */
}

static void hip_event_synchronize(hipEvent_t eve)
{
  throw_invalid_value_if(!eve, "event passed is nullptr");
  auto hip_ev = std::dynamic_pointer_cast<event>(command_cache.get(eve));
  throw_invalid_value_if(!hip_ev, "dynamic_pointer_cast failed");
  hip_ev->synchronize();
}

static float hip_event_elapsed_time(hipEvent_t start, hipEvent_t stop)
{
  throw_invalid_value_if(!start, "start event passed is nullptr");
  throw_invalid_value_if(!stop, "stop event passed is nullptr");
  auto hip_ev_start = std::dynamic_pointer_cast<event>(command_cache.get(start));
  throw_invalid_value_if(!hip_ev_start, "dynamic_pointer_cast failed");
  auto hip_ev_stop = std::dynamic_pointer_cast<event>(command_cache.get(stop));
  throw_invalid_value_if(!hip_ev_stop, "dynamic_pointer_cast failed");
  return hip_ev_start->elapsed_time(hip_ev_stop);
}

static bool hip_event_query(hipEvent_t eve)
{
  throw_invalid_value_if(!eve, "event passed is nullptr");
  auto hip_ev = std::dynamic_pointer_cast<event>(command_cache.get(eve));
  throw_invalid_value_if(!hip_ev, "dynamic_pointer_cast failed");
  return hip_ev->query();
}
} // // xrt::core::hip

// =========================================================================
//                    Event APIs implementation
// =========================================================================
hipError_t hipEventCreate(hipEvent_t* event)
{
  try {
    throw_invalid_value_if(!event, "event passed is nullptr");

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

hipError_t hipEventDestroy(hipEvent_t event)
{
  try {
    throw_invalid_value_if(!event, "event passed is nullptr");
    
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

hipError_t hipEventSynchronize(hipEvent_t event)
{
  try {
    throw_invalid_value_if(!event, "event passed is nullptr");

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

hipError_t hipEventRecord(hipEvent_t event, hipStream_t stream)
{
  try {
    throw_invalid_value_if(!event, "event passed is nullptr");
    throw_invalid_value_if(!stream, "stream passed is nullptr");

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

hipError_t hipEventQuery (hipEvent_t event)
{
  try {
    throw_invalid_value_if(!event, "event passed is nullptr");

    if (xrt::core::hip::hip_event_query(event)){
      return hipSuccess;
    }
    else {
      return hipErrorNotReady;
    }
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

hipError_t hipEventElapsedTime (float *ms, hipEvent_t start, hipEvent_t stop)
{
  try {
    throw_invalid_value_if(!start, "start event passed is nullptr");
    throw_invalid_value_if(!stop, "stop event passed is nullptr");
    throw_invalid_value_if(!ms, "the ms (elapsed time output) passed is nullptr");

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

