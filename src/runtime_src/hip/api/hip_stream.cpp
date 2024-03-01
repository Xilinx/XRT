// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "hip/core/common.h"
#include "hip/core/event.h"
#include "hip/core/stream.h"

namespace xrt::core::hip {
// In Hip, based on flags we can create default or non blocking streams.
// If application doesn't explicitly specify stream we use default stream
// for such operations. The default stream also has two modes legacy or per thread.
// Legacy default stream is also called as NULL stream. Null stream waits on all explictly
// created default streams in the same context when an operation is enqueued and explicitly
// created default streams wait on null stream in that context.
// Per thread stream is also default stream but is created per thread per context and waits
// on null stream of that context.

static stream_handle
hip_stream_create_with_flags(unsigned int flags)
{
  throw_invalid_value_if(flags != hipStreamDefault && flags != hipStreamNonBlocking,
                         "Invalid flags passed for stream creation");
  auto hip_ctx = get_current_context();
  throw_context_destroyed_if(!hip_ctx, "context is destroyed, no active context");

  std::lock_guard<std::mutex> lock(streams.mutex);
  return insert_in_map(streams.stream_cache, std::make_shared<stream>(hip_ctx, flags));
}

static void
hip_stream_destroy(hipStream_t stream)
{
  throw_invalid_handle_if(!stream, "stream is nullptr");
  throw_invalid_resource_if(stream == hipStreamPerThread, "Stream per thread can't be destroyed");

  std::lock_guard<std::mutex> lock(streams.mutex);
  streams.stream_cache.remove(stream);
}

static void
hip_stream_synchronize(hipStream_t stream)
{
  auto hip_stream = get_stream(stream);
  throw_invalid_handle_if(!hip_stream, "stream is invalid");
  hip_stream->synchronize();
  hip_stream->await_completion();
}

static void
hip_stream_wait_event(hipStream_t stream, hipEvent_t ev, unsigned int flags)
{
  throw_invalid_handle_if(flags != 0, "flags should be 0");

  auto hip_wait_stream = get_stream(stream);
  throw_invalid_resource_if(!hip_wait_stream, "stream is invalid");

  throw_invalid_handle_if(!ev, "event is nullptr");
  auto hip_event_cmd = command_cache.get(ev);

  throw_invalid_resource_if(!hip_event_cmd, "event is invalid");
  auto event_hdl = dynamic_cast<event*>(hip_event_cmd.get());
  throw_invalid_resource_if(!event_hdl, "event is invalid");

  throw_if(!event_hdl->is_recorded(), hipErrorStreamCaptureIsolation, "Event passed is not recorded");
  auto hip_event_stream = event_hdl->get_stream();

  if (hip_wait_stream == hip_event_stream) {
    hip_wait_stream->record_top_event(event_hdl);
  }
  else {
    auto wait_stream = hip_wait_stream.get();
    // create dummy event and add the event to be waited in its dep list
    auto dummy_event_hdl = static_cast<event*>(
        insert_in_map(command_cache,
                      std::make_shared<event>(std::move(hip_wait_stream))));
    dummy_event_hdl->record();
    dummy_event_hdl->add_dependency(std::move(hip_event_cmd));

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

