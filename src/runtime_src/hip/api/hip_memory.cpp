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
  static std::shared_ptr<device>
  get_current_device()
  {
    // TODO: get REAL current hip device
    auto dev = device_cache.get(0);
    if (dev == nullptr) {
      if (hipInit(0) != hipSuccess)
        throw std::runtime_error("hipInit() failed!");
      dev = device_cache.get(0);
    }
    return dev;
  }

  // Allocate memory on the device.
  static void
  hip_malloc(void** ptr, size_t size)
  {
    assert(ptr);
    assert(size > 0);

    auto dev = get_current_device();
    assert(dev);

    *ptr = nullptr;
    auto hip_mem = std::make_shared<xrt::core::hip::memory>(dev, size);
    auto address = hip_mem->get_address();
    if (address) {
      memory_database::instance().insert_addr(reinterpret_cast<uint64_t>(address), size, hip_mem);
      *ptr = reinterpret_cast<void* >(address);
    }
  }

  // Allocates device accessible host memory.
  static void
  hip_host_malloc(void** ptr, size_t size, unsigned int flags)
  {
    assert(ptr);
    assert(size > 0);

    auto dev = get_current_device();
    assert(dev);

    *ptr = nullptr;
    auto hip_mem = std::make_shared<xrt::core::hip::memory>(dev, size, flags);
    auto address = hip_mem->get_mapped_address();
    if (address) {
      memory_database::instance().insert_addr(reinterpret_cast<uint64_t>(address), size, hip_mem);
      *ptr = address;
    }
  }

  // Free memory allocated by the hipHostMalloc().
  static void
  hip_host_free(void* ptr)
  {
    // TODO check whether this memory is allocated using hipHostMalloc
    memory_database::instance().delete_addr(reinterpret_cast<uint64_t>(ptr));
  }

  // Free memory allocated by the hipMalloc().
  static void
  hip_free(void* ptr)
  {
    // TODO Check whether this memory is allocated using hipMalloc
    memory_database::instance().delete_addr(reinterpret_cast<uint64_t>(ptr));
  }

  // Register host memory so it can be accessed from the current device.
  static void
  hip_host_register(void* hostPtr, size_t size, unsigned int flags)
  {
    auto dev = get_current_device();
    assert(dev);

    auto hip_mem = std::make_shared<xrt::core::hip::memory>(dev, size, hostPtr, flags);
    auto host_addr = hip_mem->get_mapped_address();
    if (host_addr)
      memory_database::instance().insert_addr(reinterpret_cast<uint64_t>(host_addr), size, hip_mem);
  }

  // Un-register host pointer.
  static void
  hip_host_unregister(void* hostPtr)
  {
    // TODO check whether this memory is being registered using hipHostRegister
    memory_database::instance().delete_addr(reinterpret_cast<uint64_t>(hostPtr));
  }

  // Get Device pointer from Host Pointer allocated through hipHostMalloc().
  static void
  hip_host_get_device_pointer(void** devPtr, void* hstPtr, unsigned int flags)
  {
    assert(devPtr);

    *devPtr = nullptr;
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(hstPtr);
    if (hip_mem != nullptr)
    {
      // TODO add another API to return device pointer.
      *devPtr = hip_mem->get_address();
    }
  }

  static void
  hip_memcpy_host2device(void* dst, const void* src, size_t sizeBytes)
  {
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(dst);
    // TODO src can be either malloced memory or another hip_memory. Need to handle all cases
    hip_mem->write(src, sizeBytes);
  }

  static void
  hip_memcpy_host2host(void* dst, const void* src, size_t sizeBytes)
  {
    // TODO src and dst can be hip memories. Handle that case too
    memcpy(dst, src, sizeBytes);
  }

  static void
  hip_memcpy_device2host(void* dst, const void* src, size_t sizeBytes)
  {
    // TODO dest can be either malloced memory or another hip_memory. Need to handle all cases
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(src);
    hip_mem->read(dst, sizeBytes);
  }

  static void
  hip_memcpy_device2device(void* dst, const void* src, size_t sizeBytes)
  {
    auto hip_mem_dst = memory_database::instance().get_hip_mem_from_addr(dst);

    // TODO handle this case better. need to consider all cases here aswell
    hip_mem_dst->write(src, sizeBytes);
  }

  // Copy data from src to dst.
  static void
  hip_memcpy(void* dst, const void* src, size_t sizeBytes, hipMemcpyKind kind)
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
  hip_memset(void* dst, int value, size_t sizeBytes)
  {
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(dst);
    assert(hip_mem->get_type() != xrt::core::hip::memory_type::invalid);

    auto host_src = aligned_alloc(xrt_core::getpagesize(), sizeBytes);
    memset(host_src, value, sizeBytes);

    hip_mem->write(host_src, sizeBytes);

    free(host_src);
  }

} // xrt::core::hip

template<typename F> hipError_t
handle_hip_memory_error(F && f)
{
  try {
    f();
    return hipSuccess;
  } catch (const std::exception &ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorUnknown;
}

// Allocate memory on the device.
hipError_t
hipMalloc(void** ptr, size_t size)
{
  if (size == 0)
  {
    *ptr = nullptr;
    return hipSuccess;
  }
  return handle_hip_memory_error([&] { xrt::core::hip::hip_malloc(ptr, size); });
}

// Allocates device accessible host memory.
hipError_t
hipHostMalloc(void** ptr, size_t size, unsigned int flags)
{
  if (size == 0)
  {
    *ptr = nullptr;
    return hipSuccess;
  }
  return handle_hip_memory_error([&] { xrt::core::hip::hip_host_malloc(ptr, size, flags); });
}

// Free memory allocated by the hipHostMalloc().
hipError_t
hipHostFree(void* ptr)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_host_free(ptr); });
}

// Free memory allocated by the hipMalloc().
hipError_t
hipFree(void* ptr)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_free(ptr); });
}

// Register host memory so it can be accessed from the current device.
hipError_t
hipHostRegister(void* hostPtr, size_t sizeBytes, unsigned int flags)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_host_register(hostPtr, sizeBytes, flags); });
}

// Un-register host pointer.
hipError_t
hipHostUnregister(void* hostPtr)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_host_unregister(hostPtr); });
}

// Get Device pointer from Host Pointer allocated through hipHostMalloc.
hipError_t
hipHostGetDevicePointer(void** devPtr, void* hstPtr, unsigned int flags)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_host_get_device_pointer(devPtr, hstPtr, flags); });
}

// Copy data from src to dst.
hipError_t
hipMemcpy(void* dst, const void* src, size_t sizeBytes, hipMemcpyKind kind)
{
   return handle_hip_memory_error([&] { xrt::core::hip::hip_memcpy(dst, src, sizeBytes, kind); });
}

// Fills the first sizeBytes bytes of the memory area pointed to by dest with the constant byte value value.
hipError_t
hipMemset(void* dst, int value, size_t sizeBytes)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_memset(dst, value, sizeBytes); });
}

