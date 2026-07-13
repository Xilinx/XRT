// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2020 Xilinx, Inc
// Copyright (C) 2024-2026 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "core/common/module_loader.h"

#include "config_reader.h"
#include "dlfcn.h"
#include "utils.h"

#include "detail/xilinx_xrt.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string_view>

#ifdef __linux__
# include <unistd.h>
#endif

#ifndef XRT_LIB_DIR
# error "XRT_LIB_DIR is undefined"
#endif

namespace sfs = std::filesystem;

namespace {

// safe_getenv() - return env var value only when it was not smuggled across
// a privilege boundary (SWSPLAT-24084 / CWE-426).
//
// On Linux, secure_getenv() is the guard: the kernel sets AT_SECURE on
// execve() for setuid/setgid/capability-gaining transitions, and
// secure_getenv() returns nullptr in that case.  This is a kernel
// guarantee and covers the true privilege-escalation scenario.
//
// The sudo+env_keep vector (a low-privilege user sets XILINX_XRT=/tmp/evil
// then runs sudo some-xrt-tool with a sudoers env_keep rule) is a sudoers
// misconfiguration: the default sudoers policy (env_reset) strips env vars
// before exec.  It is not XRT's responsibility to compensate for a
// deliberately weakened sudo policy, and doing so (by checking
// is_elevated_process()) has the side effect of ignoring env vars that a
// root user legitimately set within an already-elevated shell (e.g. inside
// a docker container running as root).
//
// On Windows, secure_getenv() is unavailable; plain getenv() is used and
// the caller is responsible for env hygiene.
static std::string
safe_getenv(const char* name)
{
#ifdef __linux__
  // secure_getenv returns nullptr when AT_SECURE is set (setuid/capability exec).
  const char* val = ::secure_getenv(name); // NOLINT(concurrency-mt-unsafe)
  return val ? val : std::string{};
#else
  return xrt_core::utils::getenv(name);
#endif
}

static bool
is_emulation()
{
  static bool val = !safe_getenv("XCL_EMULATION_MODE").empty();
  return val;
}

static bool
is_sw_emulation()
{
  static auto xem = safe_getenv("XCL_EMULATION_MODE");
  static bool swem = xem.compare("sw_emu") == 0;
  return swem;
}

static bool
is_hw_emulation()
{
  static auto xem = safe_getenv("XCL_EMULATION_MODE");
  static bool hwem = xem.compare("hw_emu") == 0;
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

static sfs::path
get_xilinx_xrt()
{
  // XILINX_XRT is a user-controlled env var; ignore it when elevated
  // to prevent untrusted-search-path injection (SWSPLAT-24084 / CWE-426).
  sfs::path xrt(safe_getenv("XILINX_XRT"));
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

// Get list of platform repository paths from ini file and append
// default repository paths
std::vector<sfs::path>
get_platform_repo_paths()
{
  std::vector<sfs::path> paths;
  
  // Get repo path from ini file if any
  auto repo = xrt_core::config::get_platform_repo();
  std::string_view sv{repo};

  size_t pos = 0;
  while (pos < sv.size()) {
    pos = sv.find_first_not_of(":;", pos);
    if (pos == std::string_view::npos)
      break;

    // Find next delimiter
    auto end = sv.find_first_of(":;", pos);
    if (end == std::string_view::npos) {
      // No more delimiters: take the rest of the string
      paths.emplace_back(sv.substr(pos));
      break;
    }

    // Safe: end is in [pos, sv.size())
    paths.emplace_back(sv.substr(pos, end - pos));
    pos = end + 1;
  }

  // Append default path(s)
  const auto default_paths = xrt_core::detail::platform_repo_path();
  paths.insert(paths.end(), default_paths.begin(), default_paths.end());
  return paths;
}

static const std::vector<sfs::path>&
platform_repo_paths()
{
  // Cache repo paths
  static std::vector<sfs::path> paths{get_platform_repo_paths()};
  return paths;
}

// Return the full path to the file if it exists in a platform
// repository, else throw.
static sfs::path
platform_repo_path(const std::string& file)
{
  for (const auto& path : platform_repo_paths()) {
    auto xpath = path / file;
    if (sfs::exists(xpath))
      return xpath;
  }

  throw std::runtime_error("No such file or directory '" + file + "'");
}

/**
 * Refer to \ref platform_path(path) in module_loader.h
 */
static sfs::path
platform_path(const std::string& file_name)
{
  sfs::path xpath{file_name};
  if (sfs::exists(xpath))
    return xpath;

  if (!xpath.is_absolute())
    return platform_repo_path(file_name);

  throw std::runtime_error("No such file '" + xpath.string() + "'");
}

static sfs::path
module_path(const std::string& module)
{
  auto path = xilinx_xrt();
#ifdef _WIN32
  path /= module + ".dll";
#else
  path /= XRT_LIB_DIR;
  path /= "xrt/module/lib" + module + ".so." + XRT_VERSION_MAJOR;
#endif
  if (!sfs::exists(path) || !sfs::is_regular_file(path))
    throw std::runtime_error("No such library '" + path.string() + "'");

  return path;
}

static sfs::path
sdk_path(const std::string& module)
{
  // AMD_NPU_SDK_PATH is user-controlled; ignore when elevated (SWSPLAT-24084).
  sfs::path sdk(safe_getenv("AMD_NPU_SDK_PATH"));
  if (sdk.empty())
    throw std::runtime_error("AMD_NPU_SDK_PATH environment variable not set");

  // The SDK path is only applicable on Client Windows
  sdk /= module + ".dll";
  return sdk;
}

static sfs::path
shim_path()
{
  auto path = xilinx_xrt();
  auto name = shim_name();

#ifdef _WIN32
  path /= name + ".dll";
#else
  path /= XRT_LIB_DIR;
  path /= "lib" + name + ".so." + XRT_VERSION_MAJOR;
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

sdk_loader::
sdk_loader(const std::string& module_name,
           std::function<void (void*)> register_function,
           std::function<void ()> warning_function,
           std::function<int ()> error_function)
{
  if (error_function) {
    // Check prerequirements for this particular plugin.  If they are
    // not met, then return before we do any linking
    if (error_function())
      return;
  }

  auto path = sdk_path(module_name);
  auto handle = load_library(path.string());

  // Do the plugin specific functionality
  if (register_function)
    register_function(handle);

  if (warning_function)
    warning_function();

  // Explictly do not close the handle.  We need these dynamic
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
xrt_path_or_error(const std::string& file)
{
  if (file.empty())
    throw std::runtime_error("Invalid empty file name");

  std::filesystem::path path {file};

  // No absolute path
  if (path.is_absolute())
    throw std::runtime_error("Invalid path '" + path.string() + "' cannot be absolute");

  // May not contain any ".." to escape xilinx_xrt
  auto normalize = path.lexically_normal();
  if (normalize.string().find("..") != std::string::npos)
    throw std::runtime_error("Invalid path '" + normalize.string() + "' escapes xrt");

  return xilinx_xrt() / normalize;
}

sfs::path
platform_path(const std::string& file_name)
{
  return ::platform_path(file_name);
}

const std::vector<sfs::path>&
platform_repo_paths()
{
  return ::platform_repo_paths();
}

} // environment

} // xrt_core
