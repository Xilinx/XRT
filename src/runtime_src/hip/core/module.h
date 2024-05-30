// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_module_h
#define xrthip_module_h

#include "common.h"
#include "context.h"
#include "experimental/xrt_ext.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_kernel.h"

namespace xrt::core::hip {

// module_handle - opaque module handle
using module_handle = void*;

// function_handle - opaque function handle
using function_handle = void*;

// forward declaration
class function;

// hipModuleLoad load call of hip is used to load xclbin
// hipModuleLoadData call is used to load elf
// In both cases hipModule_t is returned which holds pointer to
// objects of module_xclbin/module_elf dervied from base class module
class module
{
  std::shared_ptr<context> m_ctx;
  bool m_xclbin_type;

public:
  module(std::shared_ptr<context> ctx, bool xclbin_type)
    : m_ctx{std::move(ctx)}
    , m_xclbin_type{xclbin_type}
  {}

  bool
  is_xclbin_module() const
  {
    return m_xclbin_type;
  }

  std::shared_ptr<context>
  get_context() const
  {
    return m_ctx;
  }

  virtual
  ~module() = default;
};

class module_xclbin : public module
{
  xrt::xclbin m_xclbin;
  xrt::hw_context m_hw_ctx;
  xrt_core::handle_map<function_handle, std::shared_ptr<function>> function_cache;

public:
  module_xclbin(std::shared_ptr<context> ctx, const std::string& file_name);

  void
  create_hw_context();

  function_handle
  add_function(std::shared_ptr<function> f)
  {
    return insert_in_map(function_cache, f);
  }

  std::shared_ptr<function>
  get_function(function_handle handle)
  {
    return function_cache.get(handle);
  }

  const xrt::hw_context&
  get_hw_context()
  {
    return m_hw_ctx;
  }
};

class module_elf : public module
{
  xrt::elf m_elf;
  xrt::module m_module;
  module_xclbin* m_xclbin_module;

public:
  module_elf(module_xclbin* xclbin_module, const std::string& file_name);

  module_xclbin*
  get_xclbin_module() const { return m_xclbin_module; }

  const xrt::module&
  get_xrt_module() const { return m_module; }
};

class function
{
  module_xclbin* m_module = nullptr;
  std::string m_func_name;
  xrt::ext::kernel m_kernel;

public:
  function() = default;
  function(module_xclbin* mod_hdl, const xrt::module& xrt_module, const std::string& name);

  module_xclbin*
  get_module() const
  {
    return m_module;
  }

  const xrt::ext::kernel&
  get_kernel()
  {
    return m_kernel;
  }
};

extern xrt_core::handle_map<module_handle, std::shared_ptr<module>> module_cache;
} // xrt::core::hip

#endif

