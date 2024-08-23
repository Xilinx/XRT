// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "module.h"

#include <sstream>

namespace {
static xrt::elf
create_elf(void* data, size_t size)
{
  std::istringstream stream;
  stream.rdbuf()->pubsetbuf(static_cast<char*>(data), size);
  return xrt::elf{stream};
}

static xrt::uuid
register_xclbin(std::shared_ptr<xrt::core::hip::context> ctx, const xrt::xclbin& xclbin)
{
  auto xrt_device = ctx->get_xrt_device();
  return xrt_device.register_xclbin(xclbin);
}
}

namespace xrt::core::hip {

module_xclbin::
module_xclbin(std::shared_ptr<context> ctx, const std::string& file_name)
  : module{std::move(ctx), true}
  , m_xrt_xclbin{file_name}
  , m_xrt_hw_ctx{m_ctx->get_xrt_device(), register_xclbin(m_ctx, m_xrt_xclbin)}
{}

module_xclbin::
module_xclbin(std::shared_ptr<context> ctx, void* data, size_t size)
  : module{std::move(ctx), true}
  , m_xrt_xclbin{std::vector<char>{static_cast<char*>(data), static_cast<char*>(data) + size}}
  , m_xrt_hw_ctx{m_ctx->get_xrt_device(), register_xclbin(m_ctx, m_xrt_xclbin)}
{}

module_elf::
module_elf(module_xclbin* xclbin_module, const std::string& file_name)
  : module{xclbin_module->get_context(), false}
  , m_xclbin_module{xclbin_module}
  , m_xrt_elf{file_name}
  , m_xrt_module{m_xrt_elf}
{}

module_elf::
module_elf(module_xclbin* xclbin_module, void* data, size_t size)
  : module{xclbin_module->get_context(), false}
  , m_xclbin_module{xclbin_module}
  , m_xrt_elf{create_elf(data, size)}
  , m_xrt_module{m_xrt_elf}
{}

function::
function(module_xclbin* xclbin_mod_hdl, const xrt::module& xrt_module, const std::string& name)
  : m_xclbin_module{xclbin_mod_hdl}
  , m_func_name{name}
  , m_xrt_kernel{xrt::ext::kernel{m_xclbin_module->get_hw_context(), xrt_module, name}}
{}

// Global map of modules
//we should override clang-tidy warning by adding NOLINT since module_cache is non-const parameter
xrt_core::handle_map<module_handle, std::shared_ptr<module>> module_cache; //NOLINT
}
