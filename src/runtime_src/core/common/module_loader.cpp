/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
#define XRT_CORE_COMMON_SOURCE
#include "core/common/module_loader.h"

#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>


#ifdef _WIN32
# pragma warning (disable : 4996)
#endif

namespace bfs = boost::filesystem;

namespace {

static const char*
value_or_empty(const char* cstr)
{
  return cstr ? cstr : "" ;
}

static bool
is_emulation()
{
  static bool val = (std::getenv("XCL_EMULATION_MODE") != nullptr);
  return val;
}

static bool
is_sw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool swem = xem ? (std::strcmp(xem,"sw_emu")==0) : false;
  return swem;
}

static bool
is_hw_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool hwem = xem ? (std::strcmp(xem,"hw_emu")==0) : false;
  return hwem;
}

static std::string
shim_name()
{
  if (!is_emulation())
    return "xrt_core";

  if (is_hw_emulation()) {
    auto hw_em_driver_path = xrt_core::config::get_hw_em_driver();
    return hw_em_driver_path == "null"
      ? "xrt_hwemu"
      : hw_em_driver_path;
  }

  if (is_sw_emulation()) {
    auto sw_em_driver_path = xrt_core::config::get_sw_em_driver();
    return sw_em_driver_path == "null"
      ? "xrt_swemu"
      : sw_em_driver_path;
  }

  throw std::runtime_error("Unexected error creating shim library name");
}

static bfs::path
xilinx_xrt()
{
  bfs::path xrt(value_or_empty(getenv("XILINX_XRT")));
  if (xrt.empty()){
#if defined (__aarch64__) || defined (__arm__)
    xrt = bfs::path("/usr");
#else
    throw std::runtime_error("XILINX_XRT not set");
#endif
  }

  return xrt;
}

static bfs::path
module_path(const std::string& module)
{
  auto path = xilinx_xrt();
#ifdef _WIN32
  path /= "bin/" + module + ".dll";
#else
  path /= "lib/xrt/module/lib" + module + ".so";
#endif

  if (!bfs::exists(path) || !bfs::is_regular_file(path))
    throw std::runtime_error("No such library '" + path.string() + "'");

  return path;
}

  
static bfs::path
shim_path()
{
  auto path = xilinx_xrt();
  auto name = shim_name();

#ifdef _WIN32
  path /= "bin/" + name + ".dll";
#else
  path /= "lib/lib" + name + ".so." + XRT_VERSION_MAJOR;
#endif

  if (!bfs::exists(path) || !bfs::is_regular_file(path))
    throw std::runtime_error("No such library '" + path.string() + "'");

  return path;
}

static void*
load_library(const std::string& path)
{
  if (auto handle = xrt_core::dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL))
    return handle;

  throw std::runtime_error("Failed to open library '" + path + "'\n" + xrt_core::dlerror());
}

} // end anonymous namespace

namespace xrt_core {

module_loader::
module_loader(const std::string& module_name,
              std::function<void (void*)> register_function,
              std::function<void ()> warning_function,
              std::function<int ()> error_function)
{
  if (error_function)
    // Check prerequirements for this particular plugin.  If they are
    // not met, then return before we do any linking
    if (error_function()) 
      return;

  auto path = module_path(module_name);
  auto handle = load_library(path.string());

  // Do the plugin specific functionality
  if (register_function)
    register_function(handle);

  if (warning_function)
    warning_function();

  // Explicitly do not close the handle.  We need these dynamic
  // symbols to remain open and linked through the rest of the
  // execution
}

shim_loader::
shim_loader()
{
  auto path = shim_path();
  load_library(path.string());
}

} // xrt_core
