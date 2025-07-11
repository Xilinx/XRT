// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#ifdef __linux__
#include <cerrno>
#include <cstring>
#include <ctime>
#include <dlfcn.h>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <capture/xbtracer.h>
#include <common/trace_utils.h>

namespace xrt::tools::xbtracer
{

static std::string get_so_path(const std::string &so_name)
{
  void* handle = dlopen(so_name.c_str(), RTLD_LAZY);
  if (!handle)
    xbtracer_pcritical("failed to load: \"", so_name, "\".");

  void* addr = dlsym(handle, "func_mangled_map");
  if (!addr)
    xbtracer_pcritical("failed to load symbol from \"", so_name, "\".");

  // Get the path of the shared object
  Dl_info dl_info;
  int ret = dladdr(addr, &dl_info);
  if (ret && dl_info.dli_fname) {
    std::string so_path = std::string(dl_info.dli_fname);
    xbtracer_pdebug("wrapper library is \"", so_path, "\".");
    dlclose(handle); // Close the handle
    return so_path;
  }
  else {
    dlclose(handle); // Close the handle
    xbtracer_pcritical("failed to retrieve \"", so_name, "\".");
  }
  // will not come here as xbtracer_pcritical() will abort.
  return "";
}

int launch_app(const struct tracer_arg &args)
{
  // Linux uses LD_PRELOAD to enforce preload XRT wrapper library
  std::string wrapper_path = get_so_path(WRAPPER_LIB);
  if (wrapper_path.empty())
    xbtracer_pcritical("failed to find wrapper lib \"", WRAPPER_LIB, "\".");

  xbtracer_pdebug("set LD_PRELOAD to \"", wrapper_path, "\".");
  int ret = setenv_os("LD_PRELOAD", wrapper_path.c_str());
  if (ret)
    xbtracer_pcritical("failed to set LD_PRELOAD to \"", wrapper_path, "\".");

  pid_t pid = fork();
  if (pid == 0) {
    const char* preload = getenv("LD_PRELOAD");
    if (!preload)
      xbtracer_pcritical("no LD_PRELOAD is set in child process.");
    xbtracer_pdebug("LD_PRELOAD in child process is set to: \"", std::string(preload), "\".");
    // child process to launch the target application
    std::vector<char *> c_args;
    std::vector<std::string> app_args_copy = args.target_app;
    c_args.reserve(args.target_app.size() + 1);
    for (std::string& arg : app_args_copy) {
      c_args.push_back(arg.data());
    }
    c_args.push_back(nullptr);
    execv(c_args[0], c_args.data());
    // exec() functions only return if an error has occurred.
    perror("execv");
  }
  else if (pid > 0) {
    // parent process, waits for child process to finish
    wait(nullptr);
  }
  else {
    // fork failed
    xbtracer_pcritical("failed to fork to launch target application.");
  }

  return 0;
}

} // namespace xrt::tools::xbtracer

#endif // __linux__
