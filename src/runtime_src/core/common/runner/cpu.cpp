// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#define XCL_DRIVER_DLL_EXPORT  // in same dll as exported xrt apis
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#define XRT_API_SOURCE         // in same dll as coreutil

//#define XRT_VERBOSE
#include "cpu.h"

#include "core/common/debug.h"
#include "core/common/dlfcn.h"

#include <any>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace {

using lookup_args = xrt_core::cpu::lookup_args;
using library_init_args = xrt_core::cpu::library_init_args;
using library_init_fn = xrt_core::cpu::library_init_fn;
  
// struct dllwrap - wrapper class to manange the lifetime of a loaded library
struct dllwrap
{
  using dll_guard = std::unique_ptr<void, decltype(&xrt_core::dlclose)>;
  dll_guard dll;

  explicit dllwrap(const std::filesystem::path& path)
    : dll{xrt_core::dlopen(path.string().c_str(), RTLD_NOW | RTLD_GLOBAL), xrt_core::dlclose}
  {
    if (!dll)
      throw std::runtime_error("Failed to open " + path.string() + ": " + xrt_core::dlerror());

    XRT_DEBUGF("dllwrap::dllwrap(%s) loaded\n", path.c_str());
  }
};

// Control the order of destruction of static objects. In particular
// the dlls cannot be unloaded before the library init args have been
// destroyed
static std::map<std::filesystem::path, dllwrap> s_library_handles;   // NOLINT
static std::map<std::string, lookup_args> s_function_map;            // NOLINT
static std::map<std::string, library_init_args> s_library_callbacks; // NOLINT
static std::mutex s_mutex;                                           // NOLINT

static std::filesystem::path
adjust_path(std::filesystem::path path)
{
#ifdef _WIN32
  std::filesystem::path fn = path.filename();
  fn += ".dll";
#else
  std::filesystem::path fn = "lib";
  fn += path.filename();
  fn += ".so";
#endif
  return path.replace_filename(fn);
}

static void*
open_library(std::filesystem::path dll)
{
  std::lock_guard<std::mutex> lock(s_mutex);
  if (auto it = s_library_handles.find(dll); it != s_library_handles.end())
    return it->second.dll.get();

  auto [it, inserted] = s_library_handles.emplace(dll, dllwrap{dll});
  return it->second.dll.get();
}

static const lookup_args*
lookup(const std::string& lname, const std::string& fname)
{
  XRT_DEBUGF("lookup(%s, %s)\n", lname.c_str(), fname.c_str());

  // Check if the function is already loaded
  std::lock_guard<std::mutex> lock(s_mutex);
  if (auto it = s_function_map.find(fname); it != s_function_map.end())
    return &it->second;

  // Check if the library is not already loaded in which case load and
  // initialize the library to get the callback functions
  auto cb_itr = s_library_callbacks.find(lname);
  if (cb_itr == s_library_callbacks.end()) { // load and initialize
    auto lhdl = open_library(adjust_path(lname));
    auto sym = xrt_core::dlsym(lhdl, "library_init");
    auto init = reinterpret_cast<library_init_fn>(sym);
    library_init_args args;
    init(&args);
    std::tie(cb_itr, std::ignore) = s_library_callbacks.emplace(lname, std::move(args));
  }

  // Use lookup callback function to get the function information, which
  // is cached for future reference
  auto& cb = cb_itr->second;
  lookup_args args;
  cb.lookup_fn(fname, &args);
  auto [fitr, emplaced] = s_function_map.emplace(fname, std::move(args));
  return &fitr->second;
}

} // namespace

namespace xrt_core::cpu {

class function_impl
{
  const lookup_args* m_fcn_info;
public:
  function_impl(const std::string& name, const std::string& libname)
    : m_fcn_info{lookup(libname, name)}
  {}

  uint32_t
  get_number_of_args() const
  {
    return m_fcn_info->num_args;
  }

  void
  call(std::vector<std::any>& args) const
  {
    m_fcn_info->callable(args);
  }
};

// class run - Facade for exexcuting functions within a library on the CPU
//
// Provides interface for run-time loading of a library with functions
// to be executed on the CPU by the xrt::runner class.
class run_impl
{
  std::shared_ptr<function_impl> m_fn;
  std::vector<std::any> m_args;

public:
  explicit run_impl(std::shared_ptr<function_impl> fn)
    : m_fn{std::move(fn)}
    , m_args(m_fn->get_number_of_args()) // cannot be initializer list
  {}

  void
  set_arg(int argidx, std::any value)
  {
    m_args.at(argidx) = std::move(value);
  }

  void
  execute()
  {
    // Call the function
    m_fn->call(m_args);
  }
};

////////////////////////////////////////////////////////////////
function::
function(const std::string& fname, const std::string& lname)
  : m_impl(std::make_shared<function_impl>(fname, lname))
{}

function::
~function() = default;
  
run::
run(const function& f)
  : m_impl{std::make_shared<run_impl>(f.get_handle())}
{}

run::
~run() = default;

void
run::
set_arg(int argidx, const std::any& value)
{
  m_impl->set_arg(argidx, value);
}

void
run::
execute()
{
  m_impl->execute();
}

} // namespace xrt_core::cpu
