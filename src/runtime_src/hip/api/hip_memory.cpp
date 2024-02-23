// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.

#include <string>
#include "core/common/error.h"
#include "hip/config.h"
#include "hip/core/device.h"
#include "hip/core/memory.h"
#include "hip/hip_runtime_api.h"

namespace xrt::core::hip
{

  // Allocate memory on the device.
  static void
  hip_malloc(void **ptr, size_t size)
  {
    throw std::runtime_error("Not implemented");
  }

  // Allocates device accessible host memory.
  static void
  hip_host_malloc(void **ptr, size_t size, unsigned int flags)
  {
    throw std::runtime_error("Not implemented");
  }

  // Free memory allocated by the hipHostMalloc().
  static void
  hip_host_free(void *ptr)
  {
    throw std::runtime_error("Not implemented");
  }

  // Free memory allocated by the hipMalloc().
  static void
  hip_free(void *ptr)
  {
    throw std::runtime_error("Not implemented");
  }

  // Register host memory so it can be accessed from the current device.
  static void
  hip_host_register(void *hostPtr, size_t sizeBytes, unsigned int flags)
  {
    throw std::runtime_error("Not implemented");
  }

  // Un-register host pointer.
  static void
  hip_host_unregister(void *hostPtr)
  {
    throw std::runtime_error("Not implemented");
  }

  // Get Device pointer from Host Pointer allocated through hipHostMalloc().
  static void
  hip_host_get_device_pointer(void **devPtr, void *hstPtr, unsigned int flags)
  {
    throw std::runtime_error("Not implemented");
  }

  // Copy data from src to dst.
  static void
  hip_memcpy(void *dst, const void *src, size_t sizeBytes, hipMemcpyKind kind)
  {
    throw std::runtime_error("Not implemented");
  }

} // xrt::core::hip

// Allocate memory on the device.
hipError_t
hipMalloc(void **ptr, size_t size)
{
  if (size == 0)
  {
    *ptr = nullptr;
    return hipSuccess;
  }
  try {
    xrt::core::hip::hip_malloc(ptr, size);
    return hipSuccess;
  }
  catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Allocates device accessible host memory.
hipError_t
hipHostMalloc(void **ptr, size_t size, unsigned int flags)
{
  if (size == 0)
  {
    *ptr = nullptr;
    return hipSuccess;
  }
  try {
    xrt::core::hip::hip_host_malloc(ptr, size, flags);
    return hipSuccess;
  }
  catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Free memory allocated by the hipHostMalloc().
hipError_t
hipHostFree(void *ptr)
{
  try {
    xrt::core::hip::hip_host_free(ptr);
    return hipSuccess;
  }
  catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Free memory allocated by the hipMalloc().
hipError_t
hipFree(void *ptr)
{
  try {
    xrt::core::hip::hip_free(ptr);
    return hipSuccess;
  }
  catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Register host memory so it can be accessed from the current device.
hipError_t
hipHostRegister(void *hostPtr, size_t sizeBytes, unsigned int flags)
{
  try {
    xrt::core::hip::hip_host_register(hostPtr, sizeBytes, flags);
    return hipSuccess;
  }
  catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Un-register host pointer.
hipError_t
hipHostUnregister(void *hostPtr)
{
  try {
    xrt::core::hip::hip_host_unregister(hostPtr);
    return hipSuccess;
  }
  catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Get Device pointer from Host Pointer allocated through hipHostMalloc.
hipError_t
hipHostGetDevicePointer(void **devPtr, void *hstPtr, unsigned int flags)
{
  try {
    xrt::core::hip::hip_host_get_device_pointer(devPtr, hstPtr, flags);
    return hipSuccess;
  }
  catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Copy data from src to dst.
hipError_t
hipMemcpy(void *dst, const void *src, size_t sizeBytes, hipMemcpyKind kind)
{
  try {
    xrt::core::hip::hip_memcpy(dst, src, sizeBytes, kind);
    return hipSuccess;
  }
  catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}
