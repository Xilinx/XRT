// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "core/common/error.h"
#include "core/include/experimental/xrt_system.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "hip/core/device.h"

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
}

static void
hipInit(unsigned int flags)
{
  // Flags should be zero as per Hip doc
  if (flags != 0)
    throw xrt_core::system_error(hipErrorInvalidValue, "non zero flags passed to hipinit");

  // call device_init function, device enumeration might not have happened 
  // at library load because of some exception
  // std::once_flag ensures init is called only once
  std::call_once(device_init_flag, xrt::core::hip::device_init);
}

static void
hipGetDeviceCount(int* count)
{
  if (count == nullptr)
    throw xrt_core::system_error(hipErrorInvalidValue, "arg passed is nullptr");

  // Get device count
  *count = xrt::core::hip::device_cache.size();

  if (*count < 1)
    throw xrt_core::system_error(hipErrorNoDevice, "No valid device available");
}

// Returns a handle to compute device
// Throws on error
static void
hipDeviceGet(hipDevice_t* device, int ordinal)
{
  if (device == nullptr)
    throw xrt_core::system_error(hipErrorInvalidValue, "device is nullptr");

  if (ordinal < 0 || device_cache.count(static_cast<device_handle>(ordinal)) == 0)
    throw xrt_core::system_error(hipErrorInvalidDevice, "device requested is not available");

  *device = ordinal;
}

static void
hipDeviceGetName(char* name, int len, hipDevice_t device)
{
  if (device < 0 || xrt::core::hip::device_cache.count(static_cast<xrt::core::hip::device_handle>(device)) == 0)
    throw xrt_core::system_error(hipErrorInvalidDevice, " - device requested is not available");

  if (name == nullptr || len <= 0)
    throw xrt_core::system_error(hipErrorInvalidValue, "invalid arg");

  throw std::runtime_error("Not implemented");
}

static void
hipGetDeviceProperties(hipDeviceProp_t* props, hipDevice_t device)
{
  if (props == nullptr)
    throw xrt_core::system_error(hipErrorInvalidValue, "arg passed is nullptr");

  if (device < 0 || xrt::core::hip::device_cache.count(static_cast<xrt::core::hip::device_handle>(device)) == 0)
    throw xrt_core::system_error(hipErrorInvalidDevice, "device requested is not available");

  throw std::runtime_error("Not implemented");
}

static void
hipDeviceGetUuid(hipUUID* uuid, hipDevice_t device)
{
  if (device < 0 || xrt::core::hip::device_cache.count(static_cast<xrt::core::hip::device_handle>(device)) == 0)
    throw xrt_core::system_error(hipErrorInvalidDevice, "device requested is not available");

  if (uuid == nullptr)
    throw xrt_core::system_error(hipErrorInvalidValue, "arg passed is nullptr");

  throw std::runtime_error("Not implemented");
}

static void
hipDeviceGetAttribute(int* pi, hipDeviceAttribute_t attr, int device)
{
  if (pi == nullptr)
    throw xrt_core::system_error(hipErrorInvalidValue, "arg passed is nullptr");

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
    xrt::core::hip::hipInit(flags);
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
    xrt::core::hip::hipGetDeviceCount(count);
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
    xrt::core::hip::hipDeviceGet(device, ordinal);
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
    xrt::core::hip::hipDeviceGetName(name, len, device);
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
    xrt::core::hip::hipGetDeviceProperties(props, device);
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
    xrt::core::hip::hipDeviceGetUuid(uuid, device);
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
    xrt::core::hip::hipDeviceGetAttribute(pi, attr, device);
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

