// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.

#include <string>
#include "core/common/error.h"
#include "core/common/unistd.h"
#include "hip/config.h"
#include "hip/core/device.h"
#include "hip/core/memory.h"
#include "hip/hip_runtime_api.h"

namespace xrt::core::hip
{

  //static auto sdev = std::make_shared<xrt::core::hip::device>(0);

  // Allocate memory on the device.
  static void
  hip_malloc(void **ptr, size_t size)
  {
    assert(ptr);
    assert(size > 0);

    // TODO: get current hip device
    auto dev = device_cache.get(0);
    if (dev == nullptr)
    {
      if (hipInit(0) != hipSuccess)
      {
        throw std::runtime_error("hipInit() failed!");
      }
      dev = device_cache.get(0);
    }
    assert(dev);

    auto hip_mem = std::make_shared<xrt::core::hip::memory>(size, dev);
    auto dev_addr = hip_mem->get_device_addr();
    if (dev_addr != 0)
    {
      memory_database::GetInstance()->insert_device_addr(reinterpret_cast<uint64_t>(dev_addr), size, hip_mem);
      *ptr = reinterpret_cast<void *>(dev_addr);
      return;
    }
    auto host_addr = hip_mem->get_host_addr();
    memory_database::GetInstance()->insert_host_addr(host_addr, size, hip_mem);
    *ptr = host_addr;
  }

  // Allocates device accessible host memory.
  static void
  hip_host_malloc(void **ptr, size_t size, unsigned int flags)
  {
    // TODO: get current hip device
    auto dev = device_cache.get(0);
    if (dev == nullptr)
    {
      if (hipInit(0) != hipSuccess)
      {
        throw std::runtime_error("hipInit() failed!");
      }
      dev = device_cache.get(0);
    }
    assert(dev);

    auto hip_mem = std::make_shared<xrt::core::hip::memory>(size, flags, dev);
    auto host_addr = hip_mem->get_host_addr();
    memory_database::GetInstance()->insert_host_addr(host_addr, size, hip_mem);
    *ptr = host_addr;
  }

  // Free memory allocated by the hipHostMalloc().
  static void
  hip_host_free(void *ptr)
  {
    auto hip_mem = memory_database::GetInstance()->get_hip_mem_from_host_addr(ptr);
    if (hip_mem != nullptr)
    {
      memory_database::GetInstance()->delete_host_addr(ptr);
    }
  }

  // Free memory allocated by the hipMalloc().
  static void
  hip_free(void *ptr)
  {
    auto hip_mem = memory_database::GetInstance()->get_hip_mem_from_addr(ptr);
    if (hip_mem != nullptr)
    {
      memory_database::GetInstance()->delete_addr(reinterpret_cast<uint64_t>(ptr));
    }
  }

  // Register host memory so it can be accessed from the current device.
  static void
  hip_host_register(void *hostPtr, size_t sizeBytes, unsigned int flags)
  {
    // TODO: get current hip device
    auto dev = device_cache.get(0);
    if (dev == nullptr)
    {
      if (hipInit(0) != hipSuccess)
      {
        throw std::runtime_error("hipInit() failed!");
      }
      dev = device_cache.get(0);
    }
    assert(dev);

    auto hip_mem = std::make_shared<xrt::core::hip::memory>(sizeBytes, hostPtr, flags, dev);
    auto host_addr = hip_mem->get_host_addr();
    memory_database::GetInstance()->insert_host_addr(host_addr, sizeBytes, hip_mem);
  }

  // Un-register host pointer.
  static void
  hip_host_unregister(void *hostPtr)
  {
    auto hip_mem = memory_database::GetInstance()->get_hip_mem_from_host_addr(hostPtr);
    if (hip_mem != nullptr)
    {
      memory_database::GetInstance()->delete_host_addr(hostPtr);
    }
  }

  // Get Device pointer from Host Pointer allocated through hipHostMalloc().
  static void
  hip_host_get_device_pointer(void **devPtr, void *hstPtr, unsigned int flags)
  {
    assert(devPtr);

    *devPtr = nullptr;
    auto hip_mem = memory_database::GetInstance()->get_hip_mem_from_host_addr(hstPtr);
    if (hip_mem != nullptr)
    {
      *devPtr = hip_mem->get_device_addr();
    }
  }

  static void
  hip_memcpy_host2device(void *dst, const void *src, size_t sizeBytes)
  {
    auto hip_mem = memory_database::GetInstance()->get_hip_mem_from_addr(dst);
    hip_mem->copy_from(src, sizeBytes);
  }

  static void
  hip_memcpy_host2host(void *dst, const void *src, size_t sizeBytes)
  {
    memcpy(dst, src, sizeBytes);
  }

  static void
  hip_memcpy_device2host(void *dst, const void *src, size_t sizeBytes)
  {
    auto hip_mem = memory_database::GetInstance()->get_hip_mem_from_addr(src);
    hip_mem->copy_to(dst, sizeBytes);
  }

  static void
  hip_memcpy_device2device(void *dst, const void *src, size_t sizeBytes)
  {
    auto hip_mem_src = memory_database::GetInstance()->get_hip_mem_from_addr(src);
    auto hip_mem_dst = memory_database::GetInstance()->get_hip_mem_from_addr(dst);

    hip_mem_dst->copy_from(hip_mem_src.get(), sizeBytes);
  }

  // Copy data from src to dst.
  static void
  hip_memcpy(void *dst, const void *src, size_t sizeBytes, hipMemcpyKind kind)
  {
    switch (kind)
    {
    case hipMemcpyHostToDevice:
      hip_memcpy_host2device(dst, src, sizeBytes);
      break;

    case hipMemcpyDeviceToHost:
      hip_memcpy_device2host(dst, src, sizeBytes);
      break;

    case hipMemcpyDeviceToDevice:
      hip_memcpy_device2device(dst, src, sizeBytes);
      break;

    case hipMemcpyHostToHost:
      hip_memcpy_host2host(dst, src, sizeBytes);
      break;

    default:
      break;
    };
  }

  // fill data to dst.
  static void
  hip_memset(void *dst, int value, size_t sizeBytes)
  {
    auto hip_mem = memory_database::GetInstance()->get_hip_mem_from_addr(dst);
    assert(hip_mem->get_type() != xrt::core::hip::memory_type::hip_memory_type_invalid);

    auto host_src = aligned_alloc(xrt_core::getpagesize(), sizeBytes);
    memset(host_src, value, sizeBytes);

    hip_mem->copy_from(host_src, sizeBytes);

    free(host_src);
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
  } catch (const std::exception &ex) {
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
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Free memory allocated by the hipHostMalloc().
hipError_t
hipHostFree(void *ptr)
{
  try
  {
    xrt::core::hip::hip_host_free(ptr);
    return hipSuccess;
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Free memory allocated by the hipMalloc().
hipError_t
hipFree(void *ptr)
{
  try
  {
    xrt::core::hip::hip_free(ptr);
    return hipSuccess;
  } catch (const std::exception &ex) {
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
  } catch (const std::exception &ex) {
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
  } catch (const std::exception &ex) {
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
  } catch (const std::exception &ex) {
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
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Fills the first sizeBytes bytes of the memory area pointed to by dest with the constant byte value value.
hipError_t
hipMemset(void *dst, int value, size_t sizeBytes)
{
  try {
    xrt::core::hip::hip_memset(dst, value, sizeBytes);
    return hipSuccess;
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}
