// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_RUNNER_CPU_H_
#define XRT_COMMON_RUNNER_CPU_H_
#include "core/common/config.h"
#include "runner.h"

#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <string>

namespace xrt_core::cpu {

// class function - Manage a CPU function within a library
//
// Functions are created by the xrt::runner class as part of
// initializing resources specified in a run-recipe json.
class function_impl;
class function
{
  std::shared_ptr<function_impl> m_impl;
public:
  XRT_CORE_COMMON_EXPORT
  function(const std::string& fcn, const std::string& libname);

  XRT_CORE_COMMON_EXPORT
  ~function();

  std::shared_ptr<function_impl>
  get_handle() const
  {
    return m_impl;
  }
};

// class run - Manage execution of a CPU function
//
// A run object is created by the xrt::runner class to bind arguments
// specified in run-recipe json to the function and execute it.
class run_impl;
class run
{
  std::shared_ptr<run_impl> m_impl;
 public:
  XRT_CORE_COMMON_EXPORT
  explicit run(const function&);

  XRT_CORE_COMMON_EXPORT
  ~run();

  XRT_CORE_COMMON_EXPORT
  void
  set_arg(int argidx, const std::any& value);

  XRT_CORE_COMMON_EXPORT
  void
  execute();
}; // run

} // xrt_core::cpu
#endif
