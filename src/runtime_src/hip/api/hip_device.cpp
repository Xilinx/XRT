// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
#include "core/include/xrt/experimental/xrt_system.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"

#include "hip/core/common.h"
#include "hip/core/device.h"
#include "hip/core/memory_pool.h"
#include "hip/core/module.h"

#include <cstring>
#include <mutex>
#include <string>

#ifdef _WIN32
// to disable the compiler worning C4996: 'strncpy': This function or variable may be unsafe
# pragma warning( disable : 4996)
#endif

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

static int
hip_get_device_count()
{
  // Get device count
  auto count = xrt::core::hip::device_cache.size();

  throw_if(count < 1, hipErrorNoDevice, "No valid device available");

  return static_cast<int>(count);
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

  return (xrt_core::device_query<xrt_core::query::rom_vbnv>((device_cache.get_or_error(device))->get_xrt_device().get_handle()));
}

static hipUUID
hip_device_get_uuid(hipDevice_t device)
{
  throw_invalid_device_if(check(device), "device requested is not available");

  hipUUID uid = {0};
  auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>((device_cache.get_or_error(device))->get_xrt_device().get_handle());
  std::memcpy(uid.bytes, &std::get<0>(bdf), sizeof(uint16_t));
  std::memcpy(uid.bytes + sizeof(uint16_t), &std::get<1>(bdf), sizeof(uint16_t));
  std::memcpy(uid.bytes + 2 * sizeof(uint16_t), &std::get<2>(bdf), sizeof(uint16_t));
  std::memcpy(uid.bytes + 3 * sizeof(uint16_t), &std::get<3>(bdf), sizeof(uint16_t));
  return uid;
}

static void
hip_get_device_properties(hipDeviceProp_t* props, hipDevice_t device)
{
  throw_invalid_value_if(!props, "arg passed is nullptr");
  throw_invalid_device_if(check(device), "device requested is not available");

  hipDeviceProp_t device_props = {};
  auto device_handle = (device_cache.get_or_error(device))->get_xrt_device().get_handle();
  // Query PCIe BDF (Bus/Device/Function) information
  auto uuid = xrt_core::device_query<xrt_core::query::pcie_bdf>(device_handle);
  // Query device name using rom_vbnv
  // Copy device name to device_props.name, ensuring no buffer overflow
  auto name_str = (xrt_core::device_query<xrt_core::query::rom_vbnv>(device_handle));
  std::strncpy(device_props.name, name_str.c_str(), sizeof(device_props.name));
  device_props.name[sizeof(device_props.name) - 1] = '\0';
  // Extract and assign PCI domain, bus, and device IDs from the queried PCIe BDF tuple
  device_props.pciDomainID = std::get<0>(uuid);
  device_props.pciBusID = std::get<1>(uuid);
  device_props.pciDeviceID = std::get<2>(uuid);
  device_props.canMapHostMemory = 1;
  device_props.computeMode = 0;
  device_props.concurrentKernels = 0;
#if HIP_VERSION >= 60000000
  device_props.uuid = hip_device_get_uuid(device);
  // Query if compute preemption is supported
  device_props.computePreemptionSupported = static_cast<int>(xrt_core::device_query<xrt_core::query::preemption>(device_handle));
#endif
  *props = device_props;
}

static void
hip_device_get_attribute(int* pi, hipDeviceAttribute_t attr, int device)
{
  throw_invalid_value_if(!pi, "arg passed is nullptr");
  throw_invalid_device_if(check(device), "device requested is not available");

  hipDeviceProp_t prop = {};
  hip_get_device_properties(&prop, device);
  switch (attr) {
    case hipDeviceAttributeCanMapHostMemory:
      *pi = prop.canMapHostMemory;
      break;
    case hipDeviceAttributeComputeMode:
      *pi = prop.computeMode;
      break;
    case  hipDeviceAttributeComputePreemptionSupported:
      *pi = static_cast<int>(xrt_core::device_query<xrt_core::query::preemption>((device_cache.get_or_error(device))->get_xrt_device().get_handle()));
      break;
    case hipDeviceAttributeConcurrentKernels:
      *pi = prop.concurrentKernels;
      break;
    case hipDeviceAttributePciBusId:
      *pi = prop.pciBusID;
      break;
    case hipDeviceAttributePciDeviceId:
      *pi = prop.pciDeviceID;
      break;
    case hipDeviceAttributePciDomainID:
      *pi = prop.pciDomainID;
      break;
    default:
      throw std::runtime_error("unsupported attribute type");
  }
}

// Sets thread default device
// Throws on error
static void
hip_set_device(int dev_id)
{
  throw_invalid_device_if(check(dev_id), "device to set is not available");
  tls_objs.dev_hdl = static_cast<device_handle>(dev_id);
}

const char*
hip_kernel_name_ref(const hipFunction_t f)
{
  return (reinterpret_cast<function*>(f))->get_func_name().c_str();
}
} // xrt::core::hip

// =========================================================================
// Device related apis implementation
hipError_t
hipInit(unsigned int flags)
{
  return handle_hip_func_error(__func__, hipErrorNotInitialized, [&] {
    xrt::core::hip::hip_init(flags); });
}

hipError_t
hipGetDeviceCount(int* count)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    throw_invalid_value_if(!count, "arg passed is nullptr");
    *count = xrt::core::hip::hip_get_device_count();
  });
}

hipError_t
hipDeviceGet(hipDevice_t* device, int ordinal)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    throw_invalid_value_if(!device, "device is nullptr");
    *device = xrt::core::hip::hip_device_get(ordinal);
  });
}

hipError_t
hipDeviceGetName(char* name, int len, hipDevice_t device)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    throw_invalid_value_if((!name || len <= 0), "invalid arg");

    auto name_str = xrt::core::hip::hip_device_get_name(device);
    // Only copy partial name if size of `dest` is smaller than size of `src` including
    // trailing \0
    auto cpy_size = (static_cast<size_t>(len) <= (name_str.length() + 1) ? (len - 1) : name_str.length());
    std::memcpy(name, name_str.c_str(), cpy_size);
    name[cpy_size] = '\0';
  });
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
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    xrt::core::hip::hip_get_device_properties(props, device);
  });
}

hipError_t
hipDeviceGetUuid(hipUUID* uuid, hipDevice_t device)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    throw_invalid_value_if(!uuid, "arg passed is nullptr");
    *uuid = xrt::core::hip::hip_device_get_uuid(device);
  });
}

hipError_t
hipDeviceGetAttribute(int* pi, hipDeviceAttribute_t attr, int device)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
	xrt::core::hip::hip_device_get_attribute(pi, attr, device);
  });
}

hipError_t
hipSetDevice(int device)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    xrt::core::hip::hip_set_device(device);
  });
}

const char *
hipKernelNameRef(const hipFunction_t f)
{
  const char* kname_ref = nullptr;
  hipError_t err = handle_hip_func_error(__func__, hipErrorInvalidValue, [&] {
    throw_invalid_value_if(!f, "arg passed is nullptr");
    kname_ref = xrt::core::hip::hip_kernel_name_ref(f);
  });

  if (err != hipSuccess)
    return nullptr;

  return kname_ref;
}
