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
#include "detail/xilinx_xrt.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
# pragma warning (disable : 4996)
#endif

namespace sfs = std::filesystem;

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

static bool
is_noop_emulation()
{
  static auto xem = std::getenv("XCL_EMULATION_MODE");
  static bool noop = xem ? (std::strcmp(xem,"noop")==0) : false;
  return noop;
}

static std::string
shim_name()
{
  if (!is_emulation())
#ifdef _WIN32
    return "amd_xrt_core";
#else
    return "xrt_core";
#endif

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

  if (is_noop_emulation())
    return "xrt_noop";

  throw std::runtime_error("Unexected error creating shim library name");
}

static sfs::path
get_xilinx_xrt()
{
  sfs::path xrt(value_or_empty(getenv("XILINX_XRT")));
  if (!xrt.empty())
    return xrt;

  return xrt_core::detail::xilinx_xrt();
}

static const sfs::path&
xilinx_xrt()
{
  // Cache Xilinx XRT path
  static auto xrt = get_xilinx_xrt();
  return xrt;
}

// Get list of xclbin repository paths from ini file and append
// default repository paths
std::vector<sfs::path>
get_xclbin_repo_paths()
{
  std::vector<sfs::path> paths;
  
  // Get repo path from ini file if anya
  auto repo = xrt_core::config::get_xclbin_repo();
  auto token = std::strtok(repo.data(), ":;");
  while (token) {
    paths.push_back(token);
    token = std::strtok(nullptr, ":;");
  }

  // Append default path(s)
  paths.emplace_back(xrt_core::detail::xclbin_repo_path());
  return paths;
}

static const std::vector<sfs::path>&
xclbin_repo_paths()
{
  // Cache repo paths
  static std::vector<sfs::path> paths{get_xclbin_repo_paths()};
  return paths;
}

// Return the full path to the xclbin file if it exists in an xclbin
// repository, else throw.
static sfs::path
xclbin_repo_path(const std::string& xclbin)
{
  for (const auto& path : xclbin_repo_paths()) {
    auto xpath = path / xclbin;
    if (sfs::exists(xpath) && sfs::is_regular_file(xpath))
      return xpath;
  }

  throw std::runtime_error("No such xclbin '" + xclbin + "'");
}

// Return the full path to an xclbin file if it exists, else throw.
// If the specified path is an absolute path then the function
// returns this path or throws if file does not exist.  If the path
// is relative, or just a plain file name, then the function checks
// first in current directory, then in the platform specific xclbin
// repository.
static sfs::path
xclbin_path(const std::string& xclbin)
{
  sfs::path xpath{xclbin};
  if (sfs::exists(xpath) && sfs::is_regular_file(xpath))
    return xpath;

  if (!xpath.is_absolute())
    return xclbin_repo_path(xclbin);

  throw std::runtime_error("No such xclbin '" + xpath.string() + "'");
}

static sfs::path
module_path(const std::string& module)
{
  auto path = xilinx_xrt();
#ifdef _WIN32
  path /= module + ".dll";
#else
  path /= "lib/xrt/module/lib" + module + ".so";
#endif

  if (!sfs::exists(path) || !sfs::is_regular_file(path))
    throw std::runtime_error("No such library '" + path.string() + "'");

  return path;
}
  
static sfs::path
shim_path()
{
  auto path = xilinx_xrt();
  auto name = shim_name();

#ifdef _WIN32
  path /= name + ".dll";
#else
  path /= "lib/lib" + name + ".so." + XRT_VERSION_MAJOR;
#endif

  if (!sfs::exists(path) || !sfs::is_regular_file(path))
    throw std::runtime_error("No such library '" + path.string() + "'");

  return path;
}

static std::vector<std::string>
driver_plugin_paths()
{
  std::vector<std::string> ret;
  sfs::directory_iterator p{shim_path().parent_path()};

  // All driver plug-ins are in the same directory as shim .so and with below prefix and suffix.
  const std::string pre = "libxrt_driver_";
  const std::string suf = std::string(".so.") + XRT_VERSION_MAJOR;
  while (p != sfs::directory_iterator{}) {
    const auto name = p->path().filename().string();
    if ((name.size() > (pre.size() + suf.size())) &&
      !name.compare(0, pre.size(), pre) &&
      !name.compare(name.size() - suf.size(), suf.size(), suf))
      ret.push_back(p->path().string());
    p++;
  }

  return ret;
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

driver_loader::
driver_loader()
{
  auto paths = driver_plugin_paths();

  for (const auto& p : paths)
    load_library(p);
}

namespace environment {

const sfs::path&
xilinx_xrt()
{
  return ::xilinx_xrt();
}

sfs::path
xclbin_path(const std::string& xclbin_name)
{
  return ::xclbin_path(xclbin_name);
}

const std::vector<sfs::path>&
xclbin_repo_paths()
{
  return ::xclbin_repo_paths();
}

} // environment

} // xrt_core
