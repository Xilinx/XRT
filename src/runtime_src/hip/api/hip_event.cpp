// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

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
  auto hip_stream = get_stream(stream);
  auto hip_ev = std::dynamic_pointer_cast<event>(command_cache.get(eve));
  throw_invalid_value_if(!hip_ev, "dynamic_pointer_cast failed");
  hip_ev->record(std::move(hip_stream));
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
  std::chrono::duration<double> elapsed_seconds = hip_ev_stop->get_time() - hip_ev_start->get_time();
  auto duration_in_micros = std::chrono::duration_cast<std::chrono::microseconds>(elapsed_seconds).count();
  return static_cast<float>(duration_in_micros / 1000.0); // NOLINT magic number
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
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!event, "event passed is nullptr");

    auto handle = xrt::core::hip::hip_event_create();
    *event = reinterpret_cast<hipEvent_t>(handle);
  });
}

hipError_t hipEventDestroy(hipEvent_t event)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!event, "event passed is nullptr");

    xrt::core::hip::hip_event_destroy(event);
  });
}

hipError_t hipEventSynchronize(hipEvent_t event)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!event, "event passed is nullptr");

    xrt::core::hip::hip_event_synchronize(event);
  });
}

hipError_t hipEventRecord(hipEvent_t event, hipStream_t stream)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!event, "event passed is nullptr");
    throw_invalid_value_if(!stream, "stream passed is nullptr");

    xrt::core::hip::hip_event_record(event, stream);
  });
}

hipError_t hipEventQuery (hipEvent_t event)
{
  hipError_t err = hipErrorUnknown, ret = hipSuccess;
  err = handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!event, "event passed is nullptr");

    if (xrt::core::hip::hip_event_query(event)){
      ret = hipSuccess;
    }
    else {
      ret = hipErrorNotReady;
    }
  });
  if (err != hipSuccess)
    return err;
  return ret;
}

hipError_t hipEventElapsedTime (float *ms, hipEvent_t start, hipEvent_t stop)
{
  return handle_hip_func_error(__func__, hipErrorUnknown, [&] {
    throw_invalid_value_if(!start, "start event passed is nullptr");
    throw_invalid_value_if(!stop, "stop event passed is nullptr");
    throw_invalid_value_if(!ms, "the ms (elapsed time output) passed is nullptr");

    *ms = xrt::core::hip::hip_event_elapsed_time(start, stop);
  });
}

