// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.

#include <string>
#include "core/common/error.h"
#include "core/common/unistd.h"
#include "hip/config.h"
#include "hip/core/device.h"
#include "hip/core/context.h"
#include "hip/core/memory.h"
#include "hip/hip_runtime_api.h"

namespace xrt::core::hip
{
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
    if (!address)
      throw xrt_core::system_error(hipErrorOutOfMemory, "Error allocating memory using hipMalloc");
      
    memory_database::instance().insert(reinterpret_cast<uint64_t>(address), size, hip_mem);
    *ptr = reinterpret_cast<void* >(address);
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
    auto address = hip_mem->get_address();
    if (!address)
      throw xrt_core::system_error(hipErrorOutOfMemory, "Error allocating memory using hipHostMalloc");
      
    memory_database::instance().insert(reinterpret_cast<uint64_t>(address), size, hip_mem);
    *ptr = address;
  }
  
  // Register host memory so it can be accessed from the current device.
  static void
  hip_host_register(void* host_ptr, size_t size, unsigned int flags)
  {
    auto dev = get_current_device();
    assert(dev);
    assert(host_ptr);

    auto hip_mem = std::make_shared<xrt::core::hip::memory>(dev, size, host_ptr, flags);
    auto host_addr = hip_mem->get_address();
    if (!host_addr)
      throw xrt_core::system_error(hipErrorOutOfMemory, "Error registering the host memory using hipHostRegister");

    memory_database::instance().insert(reinterpret_cast<uint64_t>(host_addr), size, hip_mem);
  }
  
  // Get Device pointer from Host Pointer allocated through hipHostMalloc().
  static void
  hip_host_get_device_pointer(void** device_ptr, void* host_ptr, unsigned int flags)
  {
    assert(device_ptr);

    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(host_ptr);
    if (!hip_mem)
      throw xrt_core::system_error(hipErrorInvalidValue, "Error getting device pointer from host_malloced memory");

    if (hip_mem->get_flags() != hipHostMallocMapped)
      throw xrt_core::system_error(hipErrorInvalidValue, "Getting device pointer is valid only for memories created with hipHostMallocMapped flag");

    *device_ptr = nullptr;
    if (hip_mem) {
      *device_ptr = hip_mem->get_device_address();
      // If device adddress is differrent than host address, insert it into database
      if (*device_ptr && *device_ptr != host_ptr)
        memory_database::instance().insert(reinterpret_cast<uint64_t>(*device_ptr), hip_mem->get_size(), hip_mem);
    }
  }

  // Free memory allocated by the hipMalloc().
  static void
  hip_free(void* ptr)
  {
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(ptr);
    if (!hip_mem || hip_mem->get_type() != memory_type::device)
      throw xrt_core::system_error(hipErrorInvalidValue, "Invalid handle passed to hipFree");

    memory_database::instance().remove(reinterpret_cast<uint64_t>(ptr));
  }
  
  // Free memory allocated by the hipHostMalloc().
  static void
  hip_host_free(void* ptr)
  {
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(ptr);
    if (!hip_mem || hip_mem->get_type() != memory_type::host)
      throw xrt_core::system_error(hipErrorInvalidValue, "Invalid handle passed to hipHostFree");

    auto device_addr = hip_mem->get_device_address();
    // if device address is differrent than host address, remove it from the map
    if ( device_addr && device_addr != ptr)
      memory_database::instance().remove(reinterpret_cast<uint64_t>(device_addr));

    memory_database::instance().remove(reinterpret_cast<uint64_t>(ptr));
  }

  // Un-register host pointer.
  static void
  hip_host_unregister(void* host_ptr)
  {
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(host_ptr);
    if (!hip_mem || hip_mem->get_type() != memory_type::registered)
      throw xrt_core::system_error(hipErrorInvalidValue, "Invalid handle passed to hipHostUnregister");

    memory_database::instance().remove(reinterpret_cast<uint64_t>(host_ptr));
  }


  static void
  hip_memcpy_host2device(void* dst, const void* src, size_t size)
  {
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(dst);
    if (!hip_mem)
      throw xrt_core::system_error(hipErrorInvalidValue, "Invalid destination handle in hipMemCpy");

    if (hip_mem) {
      // dst is device address. Get device address
      auto address = hip_mem->get_device_address();
      auto offset = reinterpret_cast<uint64_t>(dst) - reinterpret_cast<uint64_t>(address);
      hip_mem->write(src, size, 0, offset);
    }
  }

  static void
  hip_memcpy_host2host(void* dst, const void* src, size_t size)
  {
    // TODO src and dst can be hip memories. Handle that case too
    memcpy(dst, src, size);
  }

  static void
  hip_memcpy_device2host(void* dst, const void* src, size_t size)
  {
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(src);
    if (!hip_mem)
      throw xrt_core::system_error(hipErrorInvalidValue, "Invalid src handle in hipMemCpy");

    // src is device address. Get device address
    auto address = hip_mem->get_device_address();
    auto offset = reinterpret_cast<uint64_t>(src) - reinterpret_cast<uint64_t>(address);
    hip_mem->read(dst, size, 0, offset);
  }

  static void
  hip_memcpy_device2device(void* dst, const void* src, size_t size)
  {
    auto hip_mem_dst = memory_database::instance().get_hip_mem_from_addr(dst);
    if (!hip_mem_dst)
      throw xrt_core::system_error(hipErrorInvalidValue, "Invalid destination handle in hipMemCpy");

    hip_mem_dst->write(src, size);
  }

  static void
  hip_memcpy(void* dst, const void* src, size_t size, hipMemcpyKind kind)
  {
    switch (kind)
    {
      case hipMemcpyHostToDevice:
        hip_memcpy_host2device(dst, src, size);
        break;

      case hipMemcpyDeviceToHost:
        hip_memcpy_device2host(dst, src, size);
        break;

      case hipMemcpyDeviceToDevice:
        hip_memcpy_device2device(dst, src, size);
        break;

      case hipMemcpyHostToHost:
        hip_memcpy_host2host(dst, src, size);
        break;

      default:
        break;
    };
  }

  // fill data to dst.
  static void
  hip_memset(void* dst, int value, size_t size)
  {
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(dst);
    assert(hip_mem->get_type() != xrt::core::hip::memory_type::invalid);

    auto host_src = aligned_alloc(xrt_core::getpagesize(), size);
    memset(host_src, value, size);

    hip_mem->write(host_src, size);

    free(host_src);
  }

} // xrt::core::hip

template<typename F> hipError_t
handle_hip_memory_error(F && f)
{
  try {
    f();
    return hipSuccess;
  } 
  catch (const xrt_core::system_error &ex) {
    xrt_core::send_exception_message(ex.what());
    return static_cast<hipError_t> (ex.value());
  }
  catch (const std::exception &ex) {
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
hipHostRegister(void* host_ptr, size_t size, unsigned int flags)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_host_register(host_ptr, size, flags); });
}

// Un-register host pointer.
hipError_t
hipHostUnregister(void* host_ptr)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_host_unregister(host_ptr); });
}

// Get Device pointer from Host Pointer allocated through hipHostMalloc.
hipError_t
hipHostGetDevicePointer(void** device_ptr, void* host_ptr, unsigned int flags)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_host_get_device_pointer(device_ptr, host_ptr, flags); });
}

// Copy data from src to dst.
hipError_t
hipMemcpy(void* dst, const void* src, size_t size, hipMemcpyKind kind)
{
   return handle_hip_memory_error([&] { xrt::core::hip::hip_memcpy(dst, src, size, kind); });
}

// Fills the first size bytes of the memory area pointed to by dest with the constant byte value value.
hipError_t
hipMemset(void* dst, int value, size_t size)
{
  return handle_hip_memory_error([&] { xrt::core::hip::hip_memset(dst, value, size); });
}

