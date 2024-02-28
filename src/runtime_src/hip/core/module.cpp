// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "module.h"

namespace xrt::core::hip {
void
module::
create_hw_context()
{
  auto xrt_dev = m_ctx->get_xrt_device();
  auto uuid = xrt_dev.register_xclbin(m_xclbin);
  m_hw_ctx = xrt::hw_context{xrt_dev, uuid};
}

module::
module(std::shared_ptr<context> ctx, const std::string& file_name)
  : m_ctx{std::move(ctx)}
{
  m_xclbin = xrt::xclbin{file_name};
  create_hw_context();
}

module::
module(std::shared_ptr<context> ctx, void* image)
  : m_ctx{std::move(ctx)}
{
  // we trust pointer sent by application and treat
  // it as xclbin data. Application can crash/seg fault
  // when improper data is passed
  m_xclbin = xrt::xclbin{static_cast<axlf*>(image)};
  create_hw_context();
}

xrt::kernel
module::
create_kernel(std::string& name)
{
  return xrt::kernel{m_hw_ctx, name};
}

function::
function(module_handle mod_hdl, std::string&& name)
  : m_module{static_cast<module*>(mod_hdl)}
  , m_func_name{std::move(name)}
{
  if (!module_cache.count(mod_hdl))
    throw xrt_core::system_error(hipErrorInvalidResourceHandle, "module not available");


  m_kernel = m_module->create_kernel(m_func_name);
}

// Global map of modules
xrt_core::handle_map<module_handle, std::shared_ptr<module>> module_cache;
}

