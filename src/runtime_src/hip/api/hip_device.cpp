// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
#include "core/include/xrt/experimental/xrt_system.h"

#include "hip/core/common.h"
#include "hip/core/device.h"
#include "hip/core/memory_pool.h"

#include <cstring>
#include <mutex>
#include <string>

// forward declaration
namespace xrt::core::hip {
static void
device_init();
}

namespace {
//we should override clang-tidy warning by adding NOLINT since device_init_flag is non-const parameter
thread_local std::once_flag device_init_flag; //NOLINT

// Creates devices at library load
// User may not explicitly call init or device create
const struct X {
  X() noexcept {
    try {
      // needed if multi threaded
      // or else we can directly call enumerate_devices
      std::call_once(device_init_flag, xrt::core::hip::device_init);
    }
    catch (...) {}
  }
} x;
}

namespace xrt::core::hip {
// Function that calls xrt_coreutil library to get the total number of devices
static void
device_init()
{
  auto dev_count = xrt::system::enumerate_devices();

  // create all devices ahead
  // Used when user doesn't explicitly create device
  for (uint32_t i = 0; i < dev_count; i++) {
    if (device_cache.count(static_cast<device_handle>(i)) > 0)
      continue;
    auto dev = std::make_unique<xrt::core::hip::device>(i);
    device_cache.add(i, std::move(dev));
    auto default_mem_pool = std::make_shared<xrt::core::hip::memory_pool>(device_cache.get_or_error(i), MAX_MEMORY_POOL_SIZE_NPU, MEMORY_POOL_BLOCK_SIZE_NPU);
    memory_pool_db[i].push_front(default_mem_pool);
    current_memory_pool_db[i] = default_mem_pool;
    insert_in_map(mem_pool_cache, default_mem_pool);
  }
  // make first device as default device
  if (dev_count > 0)
    tls_objs.dev_hdl = static_cast<device_handle>(0);
}

static void
hip_init(unsigned int flags)
{
  // Flags should be zero as per Hip doc
  throw_invalid_value_if(flags != 0, "non zero flags passed to hipinit");

  // call device_init function, device enumeration might not have happened
  // at library load because of some exception
  // std::once_flag ensures init is called only once
  std::call_once(device_init_flag, xrt::core::hip::device_init);
}

static size_t
hip_get_device_count()
{
  // Get device count
  auto count = xrt::core::hip::device_cache.size();

  throw_if(count < 1, hipErrorNoDevice, "No valid device available");

  return count;
}

inline bool
check(int dev_id)
{
  return (dev_id < 0 || device_cache.count(static_cast<device_handle>(dev_id)) == 0);
}

// Returns a handle to compute device
// Throws on error
static int
hip_device_get(int ordinal)
{
  throw_invalid_device_if(check(ordinal), "device requested is not available");

  return ordinal;
}

static std::string
hip_device_get_name(hipDevice_t device)
{
  throw_invalid_device_if(check(device), "device requested is not available");

  throw std::runtime_error("Not implemented");
}

static hipDeviceProp_t
hip_get_device_properties(hipDevice_t device)
{
  throw_invalid_device_if(check(device), "device requested is not available");

  throw std::runtime_error("Not implemented");
}

static hipUUID
hip_device_get_uuid(hipDevice_t device)
{
  throw_invalid_device_if(check(device), "device requested is not available");

  throw std::runtime_error("Not implemented");
}

static int
hip_device_get_attribute(hipDeviceAttribute_t attr, int device)
{
  throw_invalid_device_if(check(device), "device requested is not available");

  throw std::runtime_error("Not implemented");
}
} // xrt::core::hip

// =========================================================================
// Device related apis implementation
hipError_t
hipInit(unsigned int flags)
{
  try {
    xrt::core::hip::hip_init(flags);
    return hipSuccess;
  }
  catch (const xrt_core::system_error& ex) {
    xrt_core::send_exception_message(std::string(__func__) +  " - " + ex.what());
    return static_cast<hipError_t>(ex.value());
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return hipErrorNotInitialized;
}

hipError_t
hipGetDeviceCount(size_t* count)
{
  try {
    throw_invalid_value_if(!count, "arg passed is nullptr");

    *count = xrt::core::hip::hip_get_device_count();
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
hipDeviceGet(hipDevice_t* device, int ordinal)
{
  try {
    throw_invalid_value_if(!device, "device is nullptr");

    *device = xrt::core::hip::hip_device_get(ordinal);
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
hipDeviceGetName(char* name, int len, hipDevice_t device)
{
  try {
    throw_invalid_value_if((!name || len <= 0), "invalid arg");

    auto name_str = xrt::core::hip::hip_device_get_name(device);
    // Only copy partial name if size of `dest` is smaller than size of `src` including
    // trailing \0
    auto cpy_size = (static_cast<size_t>(len) <= (name_str.length() + 1) ? (len - 1) : name_str.length());
    std::memcpy(name, name_str.c_str(), cpy_size);
    name[cpy_size] = '\0';
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

#if HIP_VERSION >= 60000000
#undef hipGetDeviceProperties

hipError_t
hipGetDeviceProperties(hipDeviceProp_t* props, hipDevice_t device)
{
  return hipErrorNotSupported;
}

hipError_t
hipGetDevicePropertiesR0600(hipDeviceProp_tR0600* props, int device)
#else
using hipDeviceProp_tR0600 = hipDeviceProp_t;

hipError_t
hipGetDevicePropertiesR0600(hipDeviceProp_tR0600* props, int device)
{
  return hipErrorNotSupported;
}

hipError_t
hipGetDeviceProperties(hipDeviceProp_t* props, hipDevice_t device)
#endif
{
  try {
    throw_invalid_value_if(!props, "arg passed is nullptr");

    *props = xrt::core::hip::hip_get_device_properties(device);
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
hipDeviceGetUuid(hipUUID* uuid, hipDevice_t device)
{
  try {
    throw_invalid_value_if(!uuid, "arg passed is nullptr");

    *uuid = xrt::core::hip::hip_device_get_uuid(device);
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
hipDeviceGetAttribute(int* pi, hipDeviceAttribute_t attr, int device)
{
  try {
    throw_invalid_value_if(!pi, "arg passed is nullptr");

    *pi = xrt::core::hip::hip_device_get_attribute(attr, device);
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
