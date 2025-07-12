#ifdef _WIN32

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include "common/trace_utils.h"
#include "xbtracer.h"
#include "core/common/windows/win_utils.h"
#include <windows.h>

namespace xrt::tools::xbtracer
{
int launch_app(const struct tracer_arg &args)
{
  STARTUPINFOA si;
  PROCESS_INFORMATION pi;
  // Initialize the STARTUPINFO structure
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);

  // Initialize the PROCESS_INFORMATION structure
  ZeroMemory(&pi, sizeof(pi));

  std::ostringstream oss;
  auto it = args.target_app.begin();
  oss << *it;
  std::advance(it, 1);
  for (; it != args.target_app.end(); ++it)
  {
    oss << " " << *it;
  }
  std::string app_cmd = oss.str();
  if (!CreateProcessA(NULL,
      (LPSTR)app_cmd.c_str(),
      NULL,                         // Process handle not inheritable
      NULL,                         // Thread handle not inheritable
      false,                        // Set handle inheritance to false
      CREATE_SUSPENDED,             // Process created in a suspended state
      NULL,                         // Use parent's environment block
      NULL,                         // Use parent's starting directory
      &si,                    // Pointer to STARTUPINFO structure
      &pi)) {          // Pointer to PROCESS_INFORMATION structure
    xbtracer_pcritical("failed to create process for target app, ", sys_dep_get_last_err_msg(), ".");
  }

  // load capturing library
  // we inject the capturing library to child process so in case when child process needs
  // to laod the library with indirect loading, it doesn't need to load it.
  // xrt_wrapper.dll depends on xrt_coreutil.dll
  int ret = inject_library(pi.hProcess, "xrt_wrapper.dll");
  if (ret)
    xbtracer_pcritical("failed to inject XRT wrapper library.");

  ResumeThread(pi.hThread);
  // Wait for the process to finish
  WaitForSingleObject(pi.hProcess, INFINITE);
  // Close handles
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);

  return 0;
}

} // namespace xrt::tools::xbtracer
#endif // _WIN32
