// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "hip/core/common.h"
#include "hip/core/context.h"
#include "hip/core/device.h"

namespace xrt::core::hip {
// In Hip, application doesn't explicitly specify the context in API calls
// so ctx stack is used for getting active ctx. App can call hipCtxCreate
// multiple times and can also use hipPrimaryCtxRetain to creates ctxs so we
// use thread local storage with ctx stack and pointer to primary ctx to decide
// active ctx to use when API call requiring ctx is encountered
// TODO: When no active ctx is available we create primary ctx on active device
// and use it

// Returns handle to context
// Throws on error
static context_handle
hip_ctx_create(unsigned int flags, hipDevice_t device)
{
  auto hip_dev = device_cache.get(static_cast<device_handle>(device));
  throw_invalid_value_if(!hip_dev, "device requested is not available");

  hip_dev->set_flags(flags);
  auto hip_ctx = std::make_shared<context>(hip_dev);
  tls_objs.ctx_stack.push(hip_ctx);  // make it current
  tls_objs.dev_hdl = device;
  // insert handle in ctx map and return handle
  return insert_in_map(context_cache, std::move(hip_ctx));
}

static void
hip_ctx_destroy(hipCtx_t ctx)
{
  auto handle = reinterpret_cast<context_handle>(ctx);
  throw_invalid_value_if(!handle, "device requested is not available");

  auto hip_ctx = context_cache.get(handle);
  throw_invalid_value_if(!hip_ctx, "context handle not found");

  // Need to remove the ctx of calling thread if its the top one
  if (!tls_objs.ctx_stack.empty() && tls_objs.ctx_stack.top().lock() == hip_ctx) {
    tls_objs.ctx_stack.pop();  // remove it is as current
  }

  context_cache.remove(handle);
}

static device_handle
hip_ctx_get_device()
{
  auto ctx = get_current_context();
  throw_context_destroyed_if(!ctx, "context is destroyed, no active context");

  return ctx->get_dev_id();
}

static void
hip_ctx_set_current(hipCtx_t ctx)
{
  if (!tls_objs.ctx_stack.empty())
    tls_objs.ctx_stack.pop();

  if (!ctx)
    return;

  auto handle = reinterpret_cast<context_handle>(ctx);
  auto hip_ctx = context_cache.get(handle);
  if (hip_ctx) {
    tls_objs.ctx_stack.push(hip_ctx);
    tls_objs.dev_hdl = hip_ctx->get_dev_id();
  }
}

// remove primary ctx as active
// release resoources if it is the last reference
static void
hip_device_primary_ctx_release(hipDevice_t dev)
{
  auto dev_hdl = static_cast<device_handle>(dev);
  auto hip_dev = device_cache.get(dev_hdl);
  throw_invalid_device_if(!hip_dev, "Invalid device");

  auto ctx = hip_dev->get_pri_ctx();
  if (!ctx)
    return;

  // decrement reference count for this primary ctx by removing its entry from map
  // primary ctx is released when all its entries from map are removed
  auto ctx_hdl =
      reinterpret_cast<context_handle>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
  context_cache.remove(ctx_hdl);
  if (tls_objs.pri_ctx_info.active && tls_objs.dev_hdl == dev_hdl) {
    tls_objs.pri_ctx_info.active = false;
    tls_objs.pri_ctx_info.ctx_hdl = nullptr;
  }
}

// create primary context on given device if not already present
// else increment reference count
context_handle
hip_device_primary_ctx_retain(device_handle dev)
{
  auto hip_dev = device_cache.get(dev);
  throw_invalid_device_if(!hip_dev, "Invalid device");

  auto hip_ctx = hip_dev->get_pri_ctx();
  // create primary context
  if (!hip_ctx) {
    hip_ctx = std::make_shared<context>(hip_dev);
    hip_dev->set_pri_ctx(hip_ctx);
  }
  // ref count of primary context is incremented by pushing on to map with
  // unqiue handle, using thread id here as primary context is unique per thread
  auto ctx_hdl =
      reinterpret_cast<context_handle>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
  context_cache.add(ctx_hdl, std::move(hip_ctx));

  tls_objs.pri_ctx_info.active = true;
  tls_objs.pri_ctx_info.ctx_hdl = ctx_hdl;
  tls_objs.dev_hdl = dev;
  return ctx_hdl;
}
} // xrt::core::hip

// =====================================================================
// Context related apis implementation
hipError_t
hipCtxCreate(hipCtx_t* ctx, unsigned int flags, hipDevice_t device)
{
  try {
    throw_invalid_value_if(!ctx, "ctx passed is nullptr");

    auto handle = xrt::core::hip::hip_ctx_create(flags, device);
    *ctx = reinterpret_cast<hipCtx_t>(handle);
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
hipCtxDestroy(hipCtx_t ctx)
{
  try {
    xrt::core::hip::hip_ctx_destroy(ctx);
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
hipCtxGetDevice(hipDevice_t* device)
{
  try {
    throw_invalid_value_if(!device, "device passed is nullptr");

    *device = xrt::core::hip::hip_ctx_get_device();
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
hipCtxSetCurrent(hipCtx_t ctx)
{
  try {
    xrt::core::hip::hip_ctx_set_current(ctx);
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
hipDevicePrimaryCtxRetain(hipCtx_t* pctx, hipDevice_t dev)
{
  try {
    throw_invalid_value_if(!pctx, "nullptr passed");

    auto handle = xrt::core::hip::hip_device_primary_ctx_retain(dev);
    *pctx = reinterpret_cast<hipCtx_t>(handle);
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
hipDevicePrimaryCtxRelease(hipDevice_t dev)
{
  try {
    xrt::core::hip::hip_device_primary_ctx_release(dev);
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

