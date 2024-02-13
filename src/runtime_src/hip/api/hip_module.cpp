// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Device, Inc. All rights reserved.

#include "core/common/error.h"

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "hip/core/module.h"

namespace xrt::core::hip {
static void
hip_module_launch_kernel(hipFunction_t f, uint32_t gridDimX, uint32_t gridDimY,
                      uint32_t gridDimZ, uint32_t blockDimX, uint32_t blockDimY,
                      uint32_t blockDimZ, uint32_t sharedMemBytes, hipStream_t hStream,
                      void** kernelParams, void** extra)
{
  if (!f)
    throw xrt_core::system_error(hipErrorInvalidResourceHandle, "function is nullptr");

  throw std::runtime_error("Not implemented");
}

static hipFunction_t
hip_module_get_function(hipModule_t hmod, const char* name)
{
  if (!name || strlen(name) == 0)
    throw xrt_core::system_error(hipErrorInvalidValue, "name is invalid");

  if (!hmod)
    throw xrt_core::system_error(hipErrorInvalidResourceHandle, "module is nullptr");

  throw std::runtime_error("Not implemented");
}

static void
hip_module_load_data_ex(hipModule_t* module, const void* image, unsigned int numOptions,
                    hipJitOption* options, void** optionsValues)
{
  if (!module)
    throw xrt_core::system_error(hipErrorInvalidResourceHandle, "module is nullptr");

  throw std::runtime_error("Not implemented");
}

static void
hip_module_load_data(hipModule_t* module, const void* image)
{
  if (!module)
    throw xrt_core::system_error(hipErrorInvalidResourceHandle, "module is nullptr");

  throw std::runtime_error("Not implemented");
}

static void
hip_module_unload(hipModule_t hmod)
{
  if (!hmod)
    throw xrt_core::system_error(hipErrorInvalidResourceHandle, "module is nullptr");

  throw std::runtime_error("Not implemented");
}

static void
hip_func_set_attribute(const void* func, hipFuncAttribute attr, int value)
{
  throw std::runtime_error("Not implemented");
}
} // // xrt::core::hip

// =========================================================================
// Module related apis implementation
hipError_t
hipModuleLaunchKernel(hipFunction_t f, uint32_t gridDimX, uint32_t gridDimY,
                      uint32_t gridDimZ, uint32_t blockDimX, uint32_t blockDimY,
                      uint32_t blockDimZ, uint32_t sharedMemBytes, hipStream_t hStream,
                      void** kernelParams, void** extra)
{
  try {
    xrt::core::hip::hip_module_launch_kernel(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY,
                                          blockDimZ, sharedMemBytes, hStream, kernelParams, extra);
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
hipModuleGetFunction(hipFunction_t* hfunc, hipModule_t hmod, const char* name)
{
  try {
    if (!hfunc)
      throw xrt_core::system_error(hipErrorInvalidHandle, "function passed is nullptr");

    *hfunc = xrt::core::hip::hip_module_get_function(hmod, name);
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
hipModuleLoadDataEx(hipModule_t* module, const void* image, unsigned int numOptions,
                    hipJitOption* options, void** optionsValues)
{
  try {
    xrt::core::hip::hip_module_load_data_ex(module, image, numOptions, options, optionsValues);
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
hipModuleLoadData(hipModule_t* module, const void* image)
{
  try {
    xrt::core::hip::hip_module_load_data(module, image);
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
hipModuleUnload(hipModule_t hmod)
{
  try {
    xrt::core::hip::hip_module_unload(hmod);
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
hipFuncSetAttribute(const void* func, hipFuncAttribute attr, int value)
{
  try {
    xrt::core::hip::hip_func_set_attribute(func, attr, value);
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
