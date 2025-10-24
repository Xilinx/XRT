// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#include <string>
#include "core/common/error.h"
#include "core/common/memalign.h"
#include "core/common/unistd.h"
#include "core/common/utils.h"
#include "hip/config.h"
#include "hip/core/device.h"
#include "hip/core/context.h"
#include "hip/core/event.h"
#include "hip/core/memory.h"
#include "hip/core/stream.h"
#include "hip/hip_runtime_api.h"

namespace xrt::core::hip
{
  // Allocate memory on the device.
  static void
  hip_malloc(void** ptr, size_t size)
  {
    throw_invalid_value_if(!ptr, "empty ptr for hip malloc.");
    throw_invalid_value_if(size <= 0, "invalid size for hip malloc.");

    auto dev = get_current_device();
    throw_invalid_device_if(!dev, "empty device for hip malloc.");

    *ptr = nullptr;
    auto hip_mem = std::make_shared<xrt::core::hip::memory>(dev, size);
    auto address = hip_mem->get_address();
    throw_if(!address, hipErrorOutOfMemory, "Error allocating memory using hipMalloc!");
      
    memory_database::instance().insert(reinterpret_cast<uint64_t>(address), size, std::move(hip_mem));
    *ptr = reinterpret_cast<void* >(address);
  }

  // Allocates device accessible host memory.
  static void
  hip_host_malloc(void** ptr, size_t size, unsigned int flags)
  {
    throw_invalid_value_if(!ptr, "empty ptr for hip malloc.");
    throw_invalid_value_if(size <= 0, "invalid size for hip malloc.");

    auto dev = get_current_device();
    throw_invalid_device_if(!dev, "empty device for hip malloc.");

    *ptr = nullptr;
    auto hip_mem = std::make_shared<xrt::core::hip::memory>(dev, size, flags);
    auto address = hip_mem->get_address();
    throw_if(!address, hipErrorOutOfMemory, "Error allocating memory using hipHostMalloc!");
      
    memory_database::instance().insert(reinterpret_cast<uint64_t>(address), size, std::move(hip_mem));
    *ptr = address;
  }
  
  // Register host memory so it can be accessed from the current device.
  static void
  hip_host_register(void* host_ptr, size_t size, unsigned int flags)
  {
    auto dev = get_current_device();
    throw_invalid_device_if(!dev, "empty device for hip malloc.");
    throw_invalid_value_if(!host_ptr, "empty host memory pointer for host memory registration.");

    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(host_ptr).first;
    throw_if(hip_mem != nullptr, hipErrorHostMemoryAlreadyRegistered, "host memory already registered.");
    hip_mem = std::make_shared<xrt::core::hip::memory>(dev, size, host_ptr, flags);
    auto host_addr = hip_mem->get_address();
    throw_if(!host_addr, hipErrorOutOfMemory, "Error registering the host memory using hipHostRegister!");

    memory_database::instance().insert(reinterpret_cast<uint64_t>(host_addr), size, std::move(hip_mem));
  }
  
  // Get Device pointer from Host Pointer allocated through hipHostMalloc().
  static void
  hip_host_get_device_pointer(void** device_ptr, void* host_ptr, unsigned int flags)
  {
    throw_invalid_value_if(!device_ptr, "empty device memory pointer handle to get device pointer.");

    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(host_ptr).first;
    throw_invalid_value_if(!hip_mem, "Error getting device pointer from host pointer.");
    // coverity[REVERSE_INULL] , preivous function already checks for nullptr
    throw_invalid_value_if(hip_mem->get_flags() != hipHostMallocMapped &&
                           hip_mem->get_flags() != hipHostRegisterMapped,
                           "Getting device pointer is valid only for memory created with hipHostMallocMapped/hipHostRegisterMapped flag!");

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
    if (!ptr)
      return;

    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(ptr).first;
    throw_invalid_handle_if(!hip_mem || hip_mem->get_type() != memory_type::device, "Invalid handle.");

    memory_database::instance().remove(reinterpret_cast<uint64_t>(ptr));
  }
  
  // Free memory allocated by the hipHostMalloc().
  static void
  hip_host_free(void* ptr)
  {
    if (!ptr)
      return;

    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(ptr).first;
    throw_invalid_handle_if(!hip_mem || hip_mem->get_type() != memory_type::host, "Invalid handle.");

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
    auto hip_mem = memory_database::instance().get_hip_mem_from_addr(host_ptr).first;
    throw_invalid_handle_if(!hip_mem || hip_mem->get_type() != memory_type::registered, "Invalid handle.");

    memory_database::instance().remove(reinterpret_cast<uint64_t>(host_ptr));
  }

  static void
  hip_memcpy_host2device(void* dst, const void* src, size_t size)
  {
    auto hip_mem_info = memory_database::instance().get_hip_mem_from_addr(dst);
    auto hip_mem_dev = hip_mem_info.first;
    auto offset = hip_mem_info.second;
    throw_invalid_handle_if(!hip_mem_dev, "Invalid destination handle.");
    throw_invalid_value_if(offset + size > hip_mem_dev->get_size(), "dst out of bound.");

    hip_mem_dev->write(src, size, 0, offset);
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
    auto hip_mem_info = memory_database::instance().get_hip_mem_from_addr(src);
    auto hip_mem_dev = hip_mem_info.first;
    auto offset = hip_mem_info.second;
    throw_invalid_handle_if(!hip_mem_dev, "Invalid source handle.");
    throw_invalid_value_if(offset + size > hip_mem_dev->get_size(), "source out of bound.");

    // src is device address. Get device address
    hip_mem_dev->read(dst, size, 0, offset);
  }

  static void
  hip_memcpy_device2device(void* dst, const void* src, size_t size)
  {
    auto dst_hip_mem_info = memory_database::instance().get_hip_mem_from_addr(dst);
    auto hip_mem_dst = dst_hip_mem_info.first;
    auto dst_offset = dst_hip_mem_info.second;
    throw_invalid_handle_if(!hip_mem_dst, "Invalid destination handle.");
    throw_invalid_value_if(dst_offset + size > hip_mem_dst->get_size(), "dst out of bound.");

    auto src_hip_mem_info = memory_database::instance().get_hip_mem_from_addr(src);
    auto hip_mem_src = src_hip_mem_info.first;
    auto src_offset = src_hip_mem_info.second;
    throw_invalid_handle_if(!hip_mem_src, "Invalid source handle.");
    throw_invalid_value_if(src_offset + size > hip_mem_src->get_size(), "src out of bound.");

    hip_mem_dst->copy(*(hip_mem_src.get()), size, src_offset, dst_offset);
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

  static void
  hip_memcpy_async(void* dst, const void* src, size_t size, hipMemcpyKind kind, hipStream_t stream)
  {
    throw_invalid_value_if(!dst, "dst is nullptr.");
    throw_invalid_value_if(!src, "src is nullptr.");

    auto hip_stream = get_stream(stream);
    throw_invalid_value_if(!hip_stream, "Invalid stream handle.");

    // ptr to a xrt::core::hip::command object could be shared between global command_cache
    // and stream::m_top_event::m_chain_of_commands of a stream object
    auto s_hdl = hip_stream.get();
    auto cmd_hdl = insert_in_map(command_cache,
                                 std::make_shared<memcpy_command>(dst, src, size, kind));
    s_hdl->enqueue(command_cache.get(cmd_hdl));
  }

  // fill data to dst.
  static void
  hip_memset(void* dst, int value, size_t size)
  {
    throw_invalid_value_if(!dst, "dst is nullptr.");
    auto hip_mem_info = memory_database::instance().get_hip_mem_from_addr(dst);
    auto hip_mem_dst = hip_mem_info.first;
    auto offset = hip_mem_info.second;
    throw_invalid_value_if(!hip_mem_dst, "Invalid destination handle.");
    throw_invalid_value_if(hip_mem_dst->get_type() == xrt::core::hip::memory_type::invalid,
                           "memory type is invalid for memset.");
    throw_invalid_value_if(offset + size > hip_mem_dst->get_size(), "dst out of bound.");

    auto host_src = xrt_core::aligned_alloc(xrt_core::getpagesize(), size);
    memset(host_src.get(), value, size);

    hip_mem_dst->write(host_src.get(), size, 0, offset);
  }

  static void
  hip_memcpy_host2device_async(hipDeviceptr_t dst, void* src, size_t size, hipStream_t stream)
  {
    throw_invalid_value_if(!src, "src is nullptr.");

    auto hip_mem_info = memory_database::instance().get_hip_mem_from_addr(dst);
    auto hip_mem_dst = hip_mem_info.first;
    auto offset = hip_mem_info.second;
    throw_invalid_value_if(!hip_mem_dst, "Invalid destination handle.");
    throw_invalid_value_if(offset + size > hip_mem_dst->get_size(), "dst out of bound.");

    auto hip_stream = get_stream(stream);
    throw_invalid_value_if(!hip_stream, "Invalid stream handle.");
   
    // ptr to a xrt::core::hip::command object could be shared between global command_cache and stream::m_top_event::m_chain_of_commands of a stream object
    auto s_hdl = hip_stream.get();
    auto cmd_hdl = insert_in_map(command_cache,
                                 std::make_shared<memcpy_command>(dst, src, size, hipMemcpyHostToDevice));
    s_hdl->enqueue(command_cache.get(cmd_hdl));
  }

  // fill data to dst async.
  template<typename T> static void
  hip_memset_async(void* dst, T value, size_t size, hipStream_t stream)
  {
    throw_invalid_value_if(!dst, "dst is nullptr.");
    auto hip_mem_info = memory_database::instance().get_hip_mem_from_addr(dst);
    auto hip_mem_dst = hip_mem_info.first;
    auto offset = hip_mem_info.second;
    throw_invalid_value_if(!hip_mem_dst, "Invalid destination handle.");
    throw_invalid_value_if(offset + size > hip_mem_dst->get_size(), "dst out of bound.");

    auto element_size = sizeof(T);

    throw_invalid_value_if((element_size != 1 && element_size != 2 && element_size != 4), "Invalid element type.");
    throw_invalid_value_if(size % element_size != 0, "Invalid size.");

    auto element_count = size / element_size;
    std::vector<T> host_vec(element_count, value);

    auto hip_stream = get_stream(stream);
    throw_invalid_value_if(!hip_stream, "Invalid stream handle.");

    // ptr to a xrt::core::hip::command object could be shared between global command_cache and stream::m_top_event::m_chain_of_commands of a stream object
    auto s_hdl = hip_stream.get();
    auto cmd_hdl = insert_in_map(command_cache,
                                 std::make_shared<copy_from_host_buffer_command<T>>(hip_mem_dst, std::move(host_vec), size, offset));
    s_hdl->enqueue(command_cache.get(cmd_hdl));
  }

  static void
  hip_mem_prefetch_async(const void* dev_ptr, size_t count, int /*device*/, hipStream_t /*stream*/)
  {
    auto hip_mem_and_off = memory_database::instance().get_hip_mem_from_addr(dev_ptr);
    auto hip_mem = hip_mem_and_off.first;
    size_t hip_mem_off = hip_mem_and_off.second;
    throw_invalid_value_if(!hip_mem, "Invalid prefetch buf address.");
    throw_invalid_value_if((hip_mem->get_size() < (hip_mem_off + count)),
                            "Invalid prefetch buf address or size.");

    // The under xrt::bo::sync() behaves the same for both TO_DEVICE or FROM_DEVICE direction.
    // we pick xclBOSyncDirection::XCL_BO_SYNC_BO_TO_DEVICE as input argument here.
    hip_mem->sync(xclBOSyncDirection::XCL_BO_SYNC_BO_TO_DEVICE, count, hip_mem_off);
  }
} // xrt::core::hip

// Allocate memory on the device.
hipError_t
hipMalloc(void** ptr, size_t size)
{
  if (size == 0)
  {
    *ptr = nullptr;
    return hipSuccess;
  }
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_malloc(ptr, size);
  });
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
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_host_malloc(ptr, size, flags);
  });
}

// Free memory allocated by the hipHostMalloc().
hipError_t
hipHostFree(void* ptr)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_host_free(ptr);
  });
}

// Free memory allocated by the hipMalloc().
hipError_t
hipFree(void* ptr)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory,
    [&] { xrt::core::hip::hip_free(ptr);
  });
}

// Register host memory so it can be accessed from the current device.
hipError_t
hipHostRegister(void* host_ptr, size_t size, unsigned int flags)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_host_register(host_ptr, size, flags);
  });
}

// Un-register host pointer.
hipError_t
hipHostUnregister(void* host_ptr)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_host_unregister(host_ptr);
  });
}

// Get Device pointer from Host Pointer allocated through hipHostMalloc.
hipError_t
hipHostGetDevicePointer(void** device_ptr, void* host_ptr, unsigned int flags)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_host_get_device_pointer(device_ptr, host_ptr, flags);
  });
}

// Copy data from src to dst.
hipError_t
hipMemcpy(void* dst, const void* src, size_t size, hipMemcpyKind kind)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_memcpy(dst, src, size, kind);
  });
}

// Async copy data from src to dst.
hipError_t
hipMemcpyAsync(void* dst, const void* src, size_t size, hipMemcpyKind kind, hipStream_t stream)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_memcpy_async(dst, src, size, kind, stream);
  });
}

// Fills the first size bytes of the memory area pointed to by dest with the constant byte value value.
hipError_t
hipMemset(void* dst, int value, size_t size)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_memset(dst, value, size);
  });
}

hipError_t
hipMemcpyHtoDAsync(hipDeviceptr_t dst, void* src, size_t size, hipStream_t stream)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_memcpy_host2device_async(dst, src, size, stream);
  });
}

// Fills the first sizeBytes bytes of the memory area pointed to by dev with the constant byte value value.
hipError_t
hipMemsetAsync(void* dst, int value, size_t size, hipStream_t stream)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_memset_async<std::uint8_t>(dst, static_cast<std::uint8_t>(value), size, stream);
  });
}

// Fills the memory area pointed to by dev with the constant integer value for specified number of times.
hipError_t
hipMemsetD32Async(void* dst, int value, size_t count, hipStream_t stream)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_memset_async<std::uint32_t>(dst, value, count*sizeof(std::uint32_t), stream);
  });
}

// Fills the memory area pointed to by dev with the constant integer value for specified number of times.
hipError_t
hipMemsetD16Async(void* dst, unsigned short value, size_t count, hipStream_t stream)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_memset_async<std::uint16_t>(dst, value, count*sizeof(std::uint16_t), stream);
  });
}

// Fills the memory area pointed to by dev with the constant integer value for specified number of times.
hipError_t
hipMemsetD8Async(void* dst, unsigned char value, size_t count, hipStream_t stream)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_memset_async<std::uint8_t>(dst, value, count*sizeof(std::uint8_t), stream);
  });
}

// Prefetches memory to the specified destination device using HIP.
hipError_t
hipMemPrefetchAsync(const void* dev_ptr, size_t count, int device, hipStream_t stream)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeMemory, [&] {
    xrt::core::hip::hip_mem_prefetch_async(dev_ptr, count, device, stream);
  });
}