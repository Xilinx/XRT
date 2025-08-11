// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrthip_module_h
#define xrthip_module_h

#include "common.h"
#include "context.h"
#include "core/include/xrt/experimental/xrt_ext.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"

#include <mutex>
#include <vector>
#include <thread>

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
protected:
  // NOLINTBEGIN
  std::shared_ptr<context> m_ctx;
  xrt_core::handle_map<function_handle, std::shared_ptr<function>> function_cache;
  // NOLINTEND

public:
  explicit module(std::shared_ptr<context> ctx)
    : m_ctx{std::move(ctx)}
  {}

  std::shared_ptr<context>
  get_context() const
  {
    return m_ctx;
  }

  virtual function_handle
  add_function(const std::string& name)
  {
    // should be called from derived class
    throw_invalid_resource_if(true, "invalid module handle passed");
    return nullptr; // to avoid compiler warning
  }

  virtual std::shared_ptr<function>
  get_function(function_handle handle) const
  {
    // should be called from derived class
    throw_invalid_resource_if(true, "invalid module handle passed");
    return nullptr; // to avoid compiler warning
  }

  virtual
  ~module() = default;
};

class module_xclbin : public module
{
  xrt::xclbin m_xrt_xclbin;
  xrt::hw_context m_xrt_hw_ctx;

public:
  module_xclbin(std::shared_ptr<context> ctx, const std::string& file_name);

  module_xclbin(std::shared_ptr<context> ctx, void* data, size_t size);

  const xrt::hw_context&
  get_hw_context() const
  {
    return m_xrt_hw_ctx;
  }
};

class module_elf : public module
{
  module_xclbin* m_xclbin_module;
  xrt::elf m_xrt_elf;
  xrt::module m_xrt_module;

public:
  module_elf(module_xclbin* xclbin_module, const std::string& file_name);

  module_elf(module_xclbin* xclbin_module, void* data, size_t size);

  module_xclbin*
  get_xclbin_module() const { return m_xclbin_module; }

  const xrt::module&
  get_xrt_module() const { return m_xrt_module; }

  function_handle
  add_function(const std::string& name) override;

  std::shared_ptr<function>
  get_function(function_handle handle) const override
  {
    return function_cache.get(handle);
  }
};

class module_full_elf : public module
{
  xrt::elf m_xrt_elf;
  xrt::hw_context m_xrt_hw_ctx;

public:
  module_full_elf(std::shared_ptr<context> ctx, const std::string& file_name);

  module_full_elf(std::shared_ptr<context> ctx, const void* data, size_t size);

  const xrt::hw_context&
  get_hw_context() const
  {
    return m_xrt_hw_ctx;
  }

  function_handle
  add_function(const std::string& name) override;

  std::shared_ptr<function>
  get_function(function_handle handle) const override
  {
    return function_cache.get(handle);
  }
};

class function
{
  module_elf* m_elf_module = nullptr;
  module_full_elf* m_full_elf_module = nullptr;
  std::vector<xrt::run> m_runs_cache; // cache for the runs to this function
  std::mutex m_runs_mutex; // lock to m_runs_cache
  std::string m_func_name;
  xrt::kernel m_xrt_kernel;

public:
  function() = default;
  function(module_elf* mod_hdl, const xrt::module& xrt_module, const std::string& name);
  function(module_full_elf* mod_hdl, const std::string& name);

  module*
  get_module() const
  {
    if (m_full_elf_module)
      return m_full_elf_module;

    return m_elf_module;
  }

  const xrt::kernel&
  get_kernel() const
  {
    return m_xrt_kernel;
  }

  xrt::run
  get_run()
  {
    {
      std::scoped_lock lock(m_runs_mutex);
      if (!m_runs_cache.empty()) {
        auto run = std::move(m_runs_cache.back());
        m_runs_cache.pop_back();
        return run;
      }
    }
    return xrt::run(m_xrt_kernel);
  }

  void
  release_run(xrt::run&& run)
  {
    std::scoped_lock lock(m_runs_mutex);
    m_runs_cache.push_back(std::move(run));
  }

  const std::string&
  get_func_name() const
  {
    return m_func_name;
  }
};

extern xrt_core::handle_map<module_handle, std::shared_ptr<module>> module_cache;
} // xrt::core::hip

#endif
