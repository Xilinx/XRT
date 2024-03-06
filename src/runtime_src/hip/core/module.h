// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_module_h
#define xrthip_module_h

#include "common.h"
#include "context.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"

namespace xrt::core::hip {

// module_handle - opaque module handle
using module_handle = void*;

// function_handle - opaque function handle
using function_handle = void*;

// forward declaration
class function;

class module
{
  std::shared_ptr<context> m_ctx;
  xrt::xclbin m_xclbin;
  xrt::hw_context m_hw_ctx;
  xrt_core::handle_map<function_handle, std::shared_ptr<function>> function_cache;

public:
  module() = default;
  module(std::shared_ptr<context> ctx, const std::string& file_name);
  module(std::shared_ptr<context> ctx, void* image);

  void
  create_hw_context();

  xrt::kernel
  create_kernel(std::string& name);

  function_handle
  add_function(std::shared_ptr<function>&& f)
  {
    return insert_in_map(function_cache, f);
  }

  std::shared_ptr<function>
  get_function(function_handle handle)
  {
    return function_cache.get(handle);
  }
};

class function
{
  module* m_module;
  std::string m_func_name;
  xrt::kernel m_kernel;

public:
  function() = default;
  function(module_handle mod_hdl, std::string&& name);

  module*
  get_module() const
  {
    return m_module;
  }

  xrt::kernel&
  get_kernel()
  {
    return m_kernel;
  }
};

extern xrt_core::handle_map<module_handle, std::shared_ptr<module>> module_cache;
} // xrt::core::hip

#endif

