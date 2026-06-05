// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_RUNNER_DETAIL_LINUX_PROCESS_H_
#define XRT_RUNNER_DETAIL_LINUX_PROCESS_H_

// This file is not to be included stand-alone

#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstring>

#include <unistd.h>
#include <sys/wait.h>

namespace xrt_core::detail {

inline int
execute_process(const std::vector<std::string>& args)
{
  if (args.empty())
    throw std::runtime_error("No command specified");

  // Convert to char* array for execvp
  std::vector<char*> c_args;
  c_args.reserve(args.size() + 1);

  for (const auto& arg : args)
    c_args.push_back(const_cast<char*>(arg.c_str()));  // NOLINT

  c_args.push_back(nullptr);

  pid_t pid = fork();

  if (pid < 0) {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    throw std::runtime_error("Failed to fork process: " + std::string(strerror(errno)));
  }
  else if (pid == 0) {
    // Child process
    execvp(c_args[0], c_args.data());

    // If execvp returns, it failed
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    std::cerr << "Failed to execute '" << args[0] << "': " << strerror(errno) << "\n";
    _exit(127); // NOLINT
  }
  else {
    // Parent process - wait for child
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
      // NOLINTNEXTLINE(concurrency-mt-unsafe)
      throw std::runtime_error("Failed to wait for process: " + std::string(strerror(errno)));

    if (WIFEXITED(status))
      return WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
      return 128 + WTERMSIG(status); // NOLINT
    else
      return -1;
  }
}

} // namespace xrt_core::detail

#endif // XRT_RUNNER_DETAIL_LINUX_PROCESS_H_
