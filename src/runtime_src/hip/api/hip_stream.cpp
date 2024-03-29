// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "hip/core/common.h"
#include "hip/core/event.h"
#include "hip/core/stream.h"

namespace xrt::core::hip {

/*
 * Diagram Explaining different kinds of streams :
 *
 ┌─────────────────────────┐             ┌─────────────────────────────┐
 │                         │   waits     │                             │
 │     Null Stream         ├────────────►│  Per Thread default stream  │
 ├─────────────────────────┤             ├─────────────────────────────┤
 │ created when application│   waits     │  created when application   │
 │ doesn't pass stream     │◄────────────┤  uses hipStreamPerThread    │
 │      explicitly         │             │       for stream            │
 ├─────────────────────────┤             ├─────────────────────────────┤
 │    stream = nullptr     │◄─────┐      │  stream = hipStreamPerThread│
 │  flag = hipStreamDefault│      │      │    flag = hipStreamDefault  │
 │                         │      │      │                             │
 └──────────────────────┬──┘      │      └─────────────────────────────┘
                   waits│      waits
                        │         │
                     ┌──▼─────────┴────────────────┐
                     │                             │
                     │      Blocking stream        │
                     ├─────────────────────────────┤
                     │    flag = hipStreamDefault  │
                     │                             │
                     └─────────────────────────────┘


                   ┌─────────────────────────────────┐
                   │                                 │
                   │    Non Blocking Stream          │
                   ├─────────────────────────────────┤
                   │    flag = hipStreamNonBlocking  │
                   │                                 │
                   │ Doesn't wait for other streams  │
                   └─────────────────────────────────┘
 * Summary :
 * Null stream waits on all Blocking, Per Thread default streams in that context
 * Streams created with Default flag wait on Null stream of that context
 * Per thread stream is created per thread per context
 * At present stream synchronization is done only when hipStreamSynchronize api
 * is explicitly called.
 * TODO : Add it at the time of command enqueue also
 */

static stream_handle
hip_stream_create_with_flags(unsigned int flags)
{
  throw_invalid_value_if(flags != hipStreamDefault && flags != hipStreamNonBlocking,
                         "Invalid flags passed for stream creation");
  auto hip_ctx = get_current_context();
  throw_context_destroyed_if(!hip_ctx, "context is destroyed, no active context");

  return insert_in_map(stream_cache, std::make_shared<stream>(hip_ctx, flags));
}

static void
hip_stream_destroy(hipStream_t stream)
{
  throw_invalid_handle_if(!stream, "stream is nullptr");
  throw_invalid_resource_if(stream == hipStreamPerThread, "Stream per thread can't be destroyed");

  stream_cache.remove(stream);
}

static void
hip_stream_synchronize(hipStream_t stream)
{
  auto hip_stream = get_stream(stream);
  throw_invalid_handle_if(!hip_stream, "stream is invalid");
  hip_stream->synchronize();
}

static void
hip_stream_wait_event(hipStream_t stream, hipEvent_t ev, unsigned int flags)
{
  throw_invalid_handle_if(flags != 0, "flags should be 0");

  auto hip_wait_stream = get_stream(stream);
  throw_invalid_resource_if(!hip_wait_stream, "stream is invalid");

  throw_invalid_handle_if(!ev, "event is nullptr");
  auto hip_event_cmd = std::dynamic_pointer_cast<event>(command_cache.get(ev));
  throw_invalid_resource_if(!hip_event_cmd, "event is invalid");

  throw_if(!hip_event_cmd->is_recorded(), hipErrorStreamCaptureIsolation, "Event passed is not recorded");
  auto hip_event_stream = hip_event_cmd->get_stream();

  // check stream on which wait is called is same as stream in which event is enqueued
  if (hip_wait_stream == hip_event_stream) {
    hip_wait_stream->record_top_event(hip_event_cmd.get());
  }
  else {
    auto wait_stream = hip_wait_stream.get();
    // create dummy event and add the event to be waited in its dep list
    auto dummy_event_hdl = static_cast<event*>(
        insert_in_map(command_cache,
                      std::make_shared<event>()));
    dummy_event_hdl->record(hip_wait_stream);
    dummy_event_hdl->add_dependency(hip_event_cmd);

    // enqueue dummy event into wait stream
    wait_stream->enqueue(command_cache.get(dummy_event_hdl));
    wait_stream->record_top_event(dummy_event_hdl);
  }
}
} // // xrt::core::hip

// =========================================================================
// Stream related apis implementation
hipError_t
hipStreamCreateWithFlags(hipStream_t* stream, unsigned int flags)
{
  try {
    throw_invalid_value_if(!stream, "stream passed is nullptr");

    auto handle = xrt::core::hip::hip_stream_create_with_flags(flags);
    *stream = reinterpret_cast<hipStream_t>(handle);
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
    xrt::core::hip::hip_stream_destroy(stream);
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
    xrt::core::hip::hip_stream_synchronize(stream);
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
    xrt::core::hip::hip_stream_wait_event(stream, event, flags);
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

