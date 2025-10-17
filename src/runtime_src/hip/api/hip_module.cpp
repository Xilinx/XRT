// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.

#include <array>

#include "hip/core/common.h"
#include "hip/core/event.h"
#include "hip/core/module.h"
#include "hip/core/stream.h"
#include "hip/hip_xrt.h"

#include <elfio/elfio.hpp>

namespace xrt::core::hip {

static void
hip_module_launch_kernel(hipFunction_t f, uint32_t /*gridDimX*/, uint32_t /*gridDimY*/,
                         uint32_t /*gridDimZ*/, uint32_t /*blockDimX*/, uint32_t /*blockDimY*/,
                         uint32_t /*blockDimZ*/, uint32_t sharedMemBytes, hipStream_t hStream,
                         void** kernelParams, void** extra)
{
  throw_invalid_resource_if(!f, "function is nullptr");
  throw_invalid_value_if(!kernelParams, "kernel parameters is nullptr");

  auto func_hdl = reinterpret_cast<function_handle>(f);
  auto hip_mod = module_cache.get(static_cast<function*>(func_hdl)->get_module());
  throw_invalid_resource_if(!hip_mod, "module associated with function is unloaded");

  auto hip_func = hip_mod->get_function(func_hdl);
  throw_invalid_resource_if(!hip_func, "invalid function passed");

  // All the RyzenAI kernels run only once, so ignoring grid and block dimensions
  // Revisit if we need to launch multiple times

  auto hip_stream = get_stream(hStream);
  throw_invalid_value_if(!hip_stream, "invalid stream handle.");
  auto s_hdl = hip_stream.get();
  auto cmd_hdl = insert_in_map(command_cache,
                               std::make_shared<kernel_start>(hip_func,
                                                              kernelParams, extra));
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

  return hip_mod->add_function(std::string{name});
}

static bool
hip_module_data_is_elf(const char *data, size_t size)
{
  constexpr std::array<unsigned char, 4> elf_header_magic = {0x7f, 'E', 'L', 'F'};
  if (size < elf_header_magic.size())
    return false;

  if (!std::memcmp(data, elf_header_magic.data(), elf_header_magic.size()))
    return true;

  return false;
}

static bool
hip_module_file_is_elf(const std::string& file_name)
{
  std::ifstream file(file_name, std::ios::in | std::ios::binary);
  throw_invalid_value_if(!file, "not able to open module file");
  std::array<char, 4> file_header{};
  file.read(file_header.data(), file_header.size());
  throw_invalid_value_if(!file, "failed to read header from module file");

  return hip_module_data_is_elf(file_header.data(), file_header.size());
}

template <typename T>
static module_handle
hip_create_module_config_param(std::shared_ptr<context> ctx, const std::string &file_name,
                               uint32_t num_config_params, const hipXrtModuleCfgParam_t *params)
{
  throw_invalid_value_if(num_config_params && !params,
			 "invalid configuration parameters passed");
  if (!num_config_params)
    return insert_in_map(module_cache, std::make_shared<T>(std::move(ctx), file_name));
  std::map<std::string, uint32_t> config_params;
  for (uint32_t i = 0; i < num_config_params; ++i)
    config_params[std::string{params[i].name}] = params[i].data;
  return insert_in_map(module_cache, std::make_shared<T>(std::move(ctx), file_name, config_params));
}

template <typename T>
static module_handle
hip_create_module_config_param(std::shared_ptr<context> ctx, void *data, size_t size,
                               uint32_t num_config_params, const hipXrtModuleCfgParam_t *params)
{
  throw_invalid_value_if(num_config_params && !params,
			 "invalid configuration parameters passed");
  if (!num_config_params)
    return insert_in_map(module_cache, std::make_shared<T>(std::move(ctx), data, size));
  std::map<std::string, uint32_t> config_params;
  for (uint32_t i = 0; i < num_config_params; ++i)
    config_params[std::string{params[i].name}] = params[i].data;
  return insert_in_map(module_cache, std::make_shared<T>(std::move(ctx), data, size,
                                                         config_params));
}

static module_handle
hip_create_top_module_config_data(const hipModuleData* config)
{
  auto ctx = get_current_context();
  throw_context_destroyed_if(!ctx, "context is destroyed, no active context");

  if (config->type == hipModuleDataFilePath) {
    std::string file_name(static_cast<const char*>(config->data));
    if (hip_module_file_is_elf(file_name))
      return hip_create_module_config_param<module_full_elf>(std::move(ctx), file_name,
                                                             config->numCfgParams,
                                                             config->cfgParams);

    return hip_create_module_config_param<module_xclbin>(std::move(ctx), file_name,
                                                         config->numCfgParams,
                                                         config->cfgParams);
  }
  else if (config->type == hipModuleDataBuffer) {
    if (hip_module_data_is_elf(static_cast<char*>(config->data), config->size))
      return hip_create_module_config_param<module_full_elf>(std::move(ctx), config->data,
                                                             config->size, config->numCfgParams,
                                                             config->cfgParams);

    return hip_create_module_config_param<module_xclbin>(std::move(ctx), config->data, config->size,
                                                         config->numCfgParams,
                                                         config->cfgParams);
  }
  throw_hip_error(hipErrorInvalidValue, "invalid module data type passed");
  // Will never reach here, this is to satisfy compiler
  return nullptr;
}

static module_handle
create_module(const hipModuleData* config)
{
  // this function can be used to load either xclbin or elf based on parent module
  // if parent module is null then buffer passed has xclbin data or full elf data,
  // else parent module will point to xclbin module already loaded and buffer passed
  // will have elf data

  throw_invalid_value_if(!config->data, "empty config data");

  if (!config->parent)
    return hip_create_top_module_config_data(config);

  // elf load
  auto hip_mod = module_cache.get(reinterpret_cast<module_handle>(config->parent));
  throw_invalid_resource_if(!hip_mod, "module not available");

  auto hip_xclbin_mod = std::dynamic_pointer_cast<module_xclbin>(hip_mod);
  throw_invalid_resource_if(!hip_xclbin_mod, "getting hip module using dynamic pointer cast failed");

  // create elf module and store it in module map
  // validity of elf_file or buffer passed is checked during xrt::elf construction
  if (config->type == hipModuleDataFilePath) {
    // data passed is file path
    return insert_in_map(module_cache,
                         std::make_shared<module_elf>(hip_xclbin_mod.get(), std::string{static_cast<char*>(config->data), config->size}));
  }
  else if (config->type == hipModuleDataBuffer) {
    // data passed is buffer
    return insert_in_map(module_cache, std::make_shared<module_elf>(hip_xclbin_mod.get(), config->data, config->size));
  }
  throw_hip_error(hipErrorInvalidValue, "invalid module data type passed");
  // Will never reach here, this is to satisfy compiler
  return nullptr;
}


static module_handle
create_full_elf_module(const std::string& fname)
{
  auto ctx = get_current_context();
  throw_context_destroyed_if(!ctx, "context is destroyed, no active context");
  // create module and store it in module map
  return insert_in_map(module_cache, std::make_shared<module_full_elf>(ctx, fname));
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
  throw_hip_error(hipErrorNotSupported, "Not implemented");
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
  return handle_hip_func_error(__func__, hipErrorLaunchFailure, [&] {
    xrt::core::hip::hip_module_launch_kernel(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY,
                                             blockDimZ, sharedMemBytes, hStream, kernelParams, extra);
  });
}

hipError_t
hipModuleGetFunction(hipFunction_t* hfunc, hipModule_t hmod, const char* name)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    throw_invalid_handle_if(!hfunc, "function passed is nullptr");

    auto handle = xrt::core::hip::hip_module_get_function(hmod, name);
    *hfunc = reinterpret_cast<hipFunction_t>(handle);
  });
}

static hipError_t
hip_module_load_data_helper(hipModule_t* module, const void* image)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    throw_invalid_resource_if(!module, "module is nullptr");

    xrt::core::hip::module_handle handle = nullptr;
    // image passed to this call is structure hipModuleData object pointer
    auto config_data = static_cast<const hipModuleData*>(image);
    handle = xrt::core::hip::create_module(config_data);
    *module = reinterpret_cast<hipModule_t>(handle);
  });
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
hipModuleLoad(hipModule_t* module, const char* fname)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    throw_invalid_resource_if(!module, "module is nullptr");

    // Treat fname passed is filepath to full ELF and
    // try creating full ELF module
    // if it throws fallback to xclbin + ELF flow
    xrt::core::hip::module_handle handle;
    if (xrt::core::hip::hip_module_file_is_elf(fname)) {
      handle = xrt::core::hip::create_full_elf_module(std::string{fname});
      *module = reinterpret_cast<hipModule_t>(handle);
      return;
    }

    handle = xrt::core::hip::create_xclbin_module(std::string{fname});
    *module = reinterpret_cast<hipModule_t>(handle);
  });
}

hipError_t
hipModuleUnload(hipModule_t hmod)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    xrt::core::hip::hip_module_unload(hmod);
  });
}

hipError_t
hipFuncSetAttribute(const void* func, hipFuncAttribute attr, int value)
{
  return handle_hip_func_error(__func__, hipErrorRuntimeOther, [&] {
    xrt::core::hip::hip_func_set_attribute(func, attr, value);
  });
}

