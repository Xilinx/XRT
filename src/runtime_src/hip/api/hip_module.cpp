// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024 Advanced Micro Devices, Inc. All rights reserved.

#include "hip/core/common.h"
#include "hip/core/event.h"
#include "hip/core/module.h"
#include "hip/core/stream.h"

namespace xrt::core::hip {

// structure that represents the config data passed to hipModuleLoadData
// xclbin_module - module handle of xclbin loaded
// elf_file - path to elf file

struct elf_config_data
{
  hipModule_t xclbin_module;
  std::string elf_file;
};

static void
hip_module_launch_kernel(hipFunction_t f, uint32_t /*gridDimX*/, uint32_t /*gridDimY*/,
                         uint32_t /*gridDimZ*/, uint32_t /*blockDimX*/, uint32_t /*blockDimY*/,
                         uint32_t /*blockDimZ*/, uint32_t sharedMemBytes, hipStream_t hStream,
                         void** kernelParams, void** /*extra*/)
{
  throw_invalid_resource_if(!f, "function is nullptr");

  auto func_hdl = reinterpret_cast<function_handle>(f);
  auto hip_mod = module_cache.get(static_cast<function*>(func_hdl)->get_module());
  throw_invalid_resource_if(!hip_mod, "module associated with function is unloaded");

  // function store module handle of xclbin
  auto hip_xclbin_mod = std::dynamic_pointer_cast<module_xclbin>(hip_mod);
  throw_invalid_resource_if(!hip_xclbin_mod, "getting hip module using dynamic pointer cast failed");

  auto hip_func = hip_xclbin_mod->get_function(func_hdl);
  throw_invalid_resource_if(!hip_func, "invalid function passed");

  // All the RyzenAI kernels run only once, so ignoring grid and block dimensions
  // Revisit if we need to launch multiple times

  auto hip_stream = get_stream(hStream);
  auto s_hdl = hip_stream.get();
  auto cmd_hdl = insert_in_map(command_cache,
                               std::make_shared<kernel_start>(hip_stream,
                                                              hip_func,
                                                              kernelParams));
  s_hdl->enqueue(command_cache.get(cmd_hdl));
}

static function_handle
hip_module_get_function(hipModule_t hmod, const char* name)
{
  throw_invalid_value_if((!name || strlen(name) == 0), "name is invalid");
  throw_invalid_resource_if(!hmod, "module is nullptr");

  auto mod_hdl = reinterpret_cast<module_handle>(hmod);
  auto hip_mod = module_cache.get(mod_hdl);
  throw_invalid_resource_if(!hip_mod, "module not available");
  // module handle passed should be elf module handle
  throw_invalid_resource_if(hip_mod->is_xclbin_module(), "invalid module handle passed");

  auto hip_elf_mod = std::dynamic_pointer_cast<module_elf>(hip_mod);
  throw_invalid_resource_if(!hip_elf_mod, "getting hip module using dynamic pointer cast failed");

  // Get xclbin module corresponding to this elf module
  auto module_xclbin = hip_elf_mod->get_xclbin_module();
  throw_invalid_resource_if(!module_cache.count(module_xclbin), "module not available");

  // create function obj and store in map maintained by xclbin module
  return module_xclbin->add_function(std::make_shared<function>(module_xclbin, hip_elf_mod->get_xrt_module(), std::string(name)));
}

static module_handle
create_elf_module(const elf_config_data* config)
{
  // elf_config_data holds module handle to xclbin loaded previously and file path to elf
  throw_invalid_resource_if(!config->xclbin_module, "module is nullptr");
  auto hip_mod = module_cache.get(reinterpret_cast<module_handle>(config->xclbin_module));
  throw_invalid_resource_if(!hip_mod, "module not available");
  throw_invalid_resource_if(!hip_mod->is_xclbin_module(), "invalid module handle passed");

  auto hip_xclbin_mod = std::dynamic_pointer_cast<module_xclbin>(hip_mod);
  throw_invalid_resource_if(!hip_xclbin_mod, "getting hip module using dynamic pointer cast failed");

  // create xrt::elf and return the module handle
  // validity of elf_file passed is checked in xrt::elf constructor
  return insert_in_map(module_cache, std::make_shared<module_elf>(hip_xclbin_mod.get(), config->elf_file));
}

static module_handle
create_xclbin_module(const std::string& fname)
{
  auto ctx = get_current_context();
  throw_context_destroyed_if(!ctx, "context is destroyed, no active context");
  // create module and store it in module map
  return insert_in_map(module_cache, std::make_shared<module_xclbin>(ctx, fname));
}

static void
hip_module_unload(hipModule_t hmod)
{
  throw_invalid_resource_if(!hmod, "module is nullptr");

  auto handle = reinterpret_cast<module_handle>(hmod);
  module_cache.remove(handle);
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
    throw_invalid_handle_if(!hfunc, "function passed is nullptr");

    auto handle = xrt::core::hip::hip_module_get_function(hmod, name);
    *hfunc = reinterpret_cast<hipFunction_t>(handle);
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

static hipError_t
hip_module_load_data_helper(hipModule_t* module, const void* image)
{
  try {
    throw_invalid_resource_if(!module, "module is nullptr");

    // image passed to this call is elf_config_data object pointer
    auto config_data = static_cast<const xrt::core::hip::elf_config_data*>(image);
    auto elf_module = xrt::core::hip::create_elf_module(config_data);
    *module = reinterpret_cast<hipModule_t>(elf_module);
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
hipModuleLoadDataEx(hipModule_t* module, const void* image, unsigned int /*numOptions*/,
                    hipJitOption* /*options*/, void** /*optionsValues*/)
{
  // Jit options are ignored for now
  // image is pointer to struct with xclbin module and elf file path
  return hip_module_load_data_helper(module, image);
}

hipError_t
hipModuleLoadData(hipModule_t* module, const void* image)
{
  // image is pointer to struct with xclbin module and elf file path
  return hip_module_load_data_helper(module, image);
}

hipError_t
hipModuleLoad(hipModule_t *module, const char *fname)
{
  try {
    throw_invalid_resource_if(!module, "module is nullptr");

    auto handle = xrt::core::hip::create_xclbin_module(std::string{fname});
    *module = reinterpret_cast<hipModule_t>(handle);
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

