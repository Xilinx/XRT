// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "hip/core/common.h"
#include "hip/core/context.h"
#include "hip/core/device.h"

namespace xrt::core::hip {
// Returns handle to context
// Throws on error
static void
hipCtxCreate(hipCtx_t* ctx, unsigned int flags, hipDevice_t device)
{
  auto hip_dev = device_cache.get(static_cast<device_handle>(device));
  if (hip_dev == nullptr) {
    throw xrt_core::system_error(hipErrorInvalidValue, "device requested is not available");
  }

  hip_dev->set_flags(flags);
  auto hip_ctx = std::make_shared<context>(hip_dev);
  context_cache.add(hip_ctx.get(), std::move(hip_ctx));
  tls_objs.ctx_stack.push(hip_ctx);  // make it current

  *ctx = reinterpret_cast<hipCtx_t>(hip_ctx.get());
}

static void
hipCtxDestroy(hipCtx_t ctx)
{
  auto handle = reinterpret_cast<context_handle>(ctx);
  if (handle == nullptr) {
    throw xrt_core::system_error(hipErrorInvalidValue, "device requested is not available");
  }

  auto hip_ctx = context_cache.get(handle);
  if (hip_ctx == nullptr)
    throw xrt_core::system_error(hipErrorInvalidValue, "context handle not found");

  // Need to remove the ctx of calling thread if its the top one
  if (!tls_objs.ctx_stack.empty() && tls_objs.ctx_stack.top() == hip_ctx) {
    tls_objs.ctx_stack.pop();  // remove it is as current
  }

  context_cache.remove(handle);
}

static void
hipCtxGetDevice(hipDevice_t* device)
{
  if (device == nullptr)
    throw xrt_core::system_error(hipErrorInvalidValue, "device passed is nullptr");
  
  auto ctx = get_current_context();
  if (!ctx)
    throw xrt_core::system_error(hipErrorInvalidValue, "Error retrieving context from device");

  *device = ctx->get_dev_id();
}

static void
hipCtxSetCurrent(hipCtx_t ctx)
{
  if (!tls_objs.ctx_stack.empty())
    tls_objs.ctx_stack.pop();

  if (ctx == nullptr)
    return;

  auto handle = reinterpret_cast<context_handle>(ctx);
  auto hip_ctx = context_cache.get(handle);
  if (hip_ctx)
    tls_objs.ctx_stack.push(hip_ctx);
}

// remove primary ctx as active
// release resoources if it is the last reference
static void
hipDevicePrimaryCtxRelease(hipDevice_t dev)
{
  auto dev_hdl = static_cast<device_handle>(dev);
  auto hip_dev = device_cache.get(dev_hdl);
  if (!hip_dev)
    throw xrt_core::system_error(hipErrorInvalidDevice, "Invalid device");

  auto ctx = hip_dev->get_primary_ctx();
  if (!ctx)
    return;
  
  // decrement reference count for this primary ctx by removing its entry from map
  // primary ctx is released when all its entries from map are removed
  auto ctx_hdl = static_cast<context_handle>(std::this_thread::get_id());
  context_cache.remove(ctx_hdl);
  if (tls_objs.pri_ctx_info.active && tls_objs.pri_ctx_info.dev_hdl == dev_hdl) {
    tls_objs.pri_ctx_info.active = false;
    tls_objs.pri_ctx_info.dev_hdl = nullptr;
    tls_objs.pri_ctx_info.ctx_hdl = nullptr;
  }
}

// create primary context on given device if not already present
// else increment reference count
static void
hipDevicePrimaryCtxRetain(hipCtx_t* pctx, hipDevice_t dev)
{
  auto hip_dev = device_cache.get(dev);
  if (!hip_dev)
    throw xrt_core::system_error(hipErrorInvalidDevice, "Invalid device");

  if (!pctx)
    throw xrt_core::system_error(hipErrorInvalidValue, "nullptr passed");

  auto dev_hdl = hip_dev.get();
  auto hip_ctx = hip_dev->get_pri_ctx();
  // create primary context
  if (!hip_ctx) {
    hip_ctx = std::make_shared<context>(hip_dev);
    hip_dev->set_pri_ctx(hip_ctx);
  }
  // ref count of primary context is incremented by pushing on to map with 
  // unqiue handle, using thread id here as primary context is unique per thread
  auto ctx_hdl = static_cast<context_handle>(std::this_thread::get_id());
  context_cache.add(ctx_hdl, std::move(hip_ctx));
  
  tls_objs.pri_ctx_info.active = true;
  tls_objs.pri_ctx_info.ctx_hdl = ctx_hdl;
  tls_objs.pri_ctx_info.dev_hdl = dev_hdl;
  *pctx = reinterpret_cast<hipCtx_t>(hip_ctx.get());
}
} // xrt::core::hip

// =====================================================================
// Context related apis implementation
hipError_t 
hipCtxCreate(hipCtx_t* ctx, unsigned int flags, hipDevice_t device)
{
  try {
    xrt::core::hip::hipCtxCreate(ctx, flags, device);
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
    xrt::core::hip::hipCtxDestroy(ctx);
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
    xrt::core::hip::hipCtxGetDevice(device);
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
    xrt::core::hip::hipCtxSetCurrent(ctx);
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
    xrt::core::hip::hipDevicePrimaryCtxRetain(pctx, dev);
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
    xrt::core::hip::hipDevicePrimaryCtxRelease(dev);
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
