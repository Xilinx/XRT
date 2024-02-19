// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "core/common/error.h"
#include "core/include/experimental/xrt_system.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "hip/core/common.h"
#include "hip/core/device.h"

#include <cstring>
#include <mutex>
#include <string>

// forward declaration
namespace xrt::core::hip {
static void
device_init();
}

namespace {
std::once_flag device_init_flag;

// Creates devices at library load
// User may not explicitly call init or device create
struct X {
  X()
  {
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
    auto dev = std::make_shared<xrt::core::hip::device>(i);
    device_cache.add(i, std::move(dev));
  }
  // make first device as default device
  if (dev_count > 0)
    tls_objs.dev_hdl = static_cast<device_handle>(0);
}

static void
hip_init(unsigned int flags)
{
  // Flags should be zero as per Hip doc
  if (flags != 0)
    throw xrt_core::system_error(hipErrorInvalidValue, "non zero flags passed to hipinit");

  // call device_init function, device enumeration might not have happened
  // at library load because of some exception
  // std::once_flag ensures init is called only once
  std::call_once(device_init_flag, xrt::core::hip::device_init);
}

static int
hip_get_device_count()
{
  // Get device count
  auto count = xrt::core::hip::device_cache.size();

  if (count < 1)
    throw xrt_core::system_error(hipErrorNoDevice, "No valid device available");

  return count;
}

// Returns a handle to compute device
// Throws on error
static int
hip_device_get(int ordinal)
{
  if (ordinal < 0 || device_cache.count(static_cast<device_handle>(ordinal)) == 0)
    throw xrt_core::system_error(hipErrorInvalidDevice, "device requested is not available");

  return ordinal;
}

static std::string
hip_device_get_name(hipDevice_t device)
{
  if (device < 0 || xrt::core::hip::device_cache.count(static_cast<xrt::core::hip::device_handle>(device)) == 0)
    throw xrt_core::system_error(hipErrorInvalidDevice, " - device requested is not available");

  throw std::runtime_error("Not implemented");
}

static hipDeviceProp_t
hip_get_device_properties(hipDevice_t device)
{
  if (device < 0 || xrt::core::hip::device_cache.count(static_cast<xrt::core::hip::device_handle>(device)) == 0)
    throw xrt_core::system_error(hipErrorInvalidDevice, "device requested is not available");

  throw std::runtime_error("Not implemented");
}

static hipUUID
hip_device_get_uuid(hipDevice_t device)
{
  if (device < 0 || xrt::core::hip::device_cache.count(static_cast<xrt::core::hip::device_handle>(device)) == 0)
    throw xrt_core::system_error(hipErrorInvalidDevice, "device requested is not available");

  throw std::runtime_error("Not implemented");
}

static int
hip_device_get_attribute(hipDeviceAttribute_t attr, int device)
{
  if (device < 0 || xrt::core::hip::device_cache.count(static_cast<xrt::core::hip::device_handle>(device)) == 0)
    throw xrt_core::system_error(hipErrorInvalidDevice, "device requested is not available");

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
hipGetDeviceCount(int* count)
{
  try {
    if (!count)
      throw xrt_core::system_error(hipErrorInvalidValue, "arg passed is nullptr");

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
    if (!device)
      throw xrt_core::system_error(hipErrorInvalidValue, "device is nullptr");

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
    if (!name || len <= 0)
      throw xrt_core::system_error(hipErrorInvalidValue, "invalid arg");

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

hipError_t
hipGetDeviceProperties(hipDeviceProp_t* props, hipDevice_t device)
{
  try {
    if (!props)
      throw xrt_core::system_error(hipErrorInvalidValue, "arg passed is nullptr");

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
    if (!uuid)
      throw xrt_core::system_error(hipErrorInvalidValue, "arg passed is nullptr");

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
    if (!pi)
      throw xrt_core::system_error(hipErrorInvalidValue, "arg passed is nullptr");

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

