// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_RUNNER_RUNNER_H_
#define XRT_COMMON_RUNNER_RUNNER_H_
#include "xrt/detail/config.h"
#include "xrt/detail/pimpl.h"
#include "xrt/experimental/xrt_exception.h"

#include <any>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace xrt {
class device;
class bo;
}

namespace xrt_core {

/**
 * class runner - A class to execute a run recipe json
 */
class runner_impl;
class runner : public xrt::detail::pimpl<runner_impl>
{
public:
  class error_impl;
  class error : public xrt::detail::pimpl<error_impl>, public xrt::exception
  {
  public:
    XRT_API_EXPORT
    explicit
    error(const std::string& msg);

    XRT_API_EXPORT
    const char*
    what() const noexcept override;
  };

  class json_error : public error
  {
    using error::error;
  };

  class recipe_error : public error
  {
    using error::error;
  };

  class profile_error : public error
  {
    using error::error;
  };

  class repo_error : public error
  {
    using error::error;
  };

  class hwctx_error : public error
  {
    using error::error;
  };

  class validation_error : public profile_error
  {
    using profile_error::profile_error;
  };

public:
  /**
   * artifacts_repository - A map of artifacts
   *
   * The runner can be constructed with an artifacts repository, in
   * which case the recipe references are looked up in the artifacts are
   * looked up in the repository rather than from disk.
   */
  using artifacts_repository = std::map<std::string, std::vector<char>>;

  // ctor - Create runner from a recipe json.
  // Any artifacts referenced by the recipe are looked up in the
  // current directory.
  XRT_API_EXPORT
  runner(const xrt::device& device, const std::string& recipe);

  // ctor - Create runner from a recipe json and path to directory
  // with artifacts
  XRT_API_EXPORT
  runner(const xrt::device& device, const std::string& recipe,
         const std::filesystem::path& artifacts_dir);

  // ctor - Create runner from a recipe json and artifacts repository
  // The repo is not copied so the lifetime of the repo must extend
  // the lifetime of the runner.
  XRT_API_EXPORT
  runner(const xrt::device& device, const std::string& recipe,
         const artifacts_repository&);

  // ctor - Create runner from a recipe json and execution profile json
  // Any artifacts referenced by recipe and profile are looked up in
  // the current directory.
  XRT_API_EXPORT
  runner(const xrt::device& device, const std::string& recipe, const std::string& profile);

  // ctor - Create runner from a recipe json and execution profile
  // json and path to directory with artifacts.
  XRT_API_EXPORT
  runner(const xrt::device& device, const std::string& recipe, const std::string& profile,
         const std::filesystem::path& artifacts_dir);

  // ctor - Create runner from a recipe json and execution profile
  // json and artifacts repository.  The repo is not copied so the
  // lifetime of the repo must extend the lifetime of the runner.
  XRT_API_EXPORT
  runner(const xrt::device& device, const std::string& recipe, const std::string& profile,
         const artifacts_repository&);

  // bind_input() - Bind a buffer object to an input tensor
  XRT_API_EXPORT
  void
  bind_input(const std::string& name, const xrt::bo& bo);

  // bind_output() - Bind a buffer object to an output tensor
  XRT_API_EXPORT
  void
  bind_output(const std::string& name, const xrt::bo& bo);

  // bind() - Bind a buffer object to a tensor
  XRT_API_EXPORT
  void
  bind(const std::string& name, const xrt::bo& bo);

  // execute() - Execute the runner
  XRT_API_EXPORT
  void
  execute();

  // wait() - Wait for the execution to complete
  XRT_API_EXPORT
  void
  wait();

  // get_report() - Get a runner report as a json string
  // The schema of the report is TBD
  XRT_API_EXPORT
  std::string
  get_report();
};

/**
 * The xrt::runner supports execution of CPU functions as well
 * as xrt::kernel objects.
 *
 * The CPU functions are implemented in runtime loaded dynamic
 * libraries. A library must define and export a function that
 * initializes a callback structure with a lookup function.
 *
 * The signature of the lookup function must be
 * @code
 *  void lookup_fn(const std::string& name, xrt::cpu::lookup_args* args)
 * @endcode
 * where the name is the name of the function to lookup and args is a
 * structure that the lookup function must populate with the function
 * information.
 *
 * The arguments to the CPU functions are elided via std::any and
 * the signature of the CPU functions is fixed to
 * @code
 *  void cpu_function(std::vector<std::any>& args)
 * @endcode
 * Internally, the CPU library unwraps the arguments and calls the
 * actual function.
 */  
namespace cpu {

/**
 * struct lookup_args - argument structure for the lookup function
 *
 * The lookup function takes as arguments the name of the function
 * to lookup along with lookup_args to be populated with information
 * about the function.
 *
 * @num_args - number of arguments to function
 * @callable - a C++ function object wrapping the function
 *
 * The callable library functions uses type erasure on their arguments
 * through a std::vector of std::any objects.  The callable must
 * unwrap the std::any objects to its expected type, which is
 * cumbersome, but type safe. The type erased arguments allow the
 * runner to be generic and not tied to a specific function signature.
*/
struct lookup_args
{
  std::uint32_t num_args {0};
  std::function<void(std::vector<std::any>&)> callable;
};

/**
 * struct library_init_args - argument structure for libray initialization
 *
 * The library initialization function is the only function exported
 * from the run time loaded library.  The library initialization
 * function is called by the runner when a resource references a
 * function in a library and the library is not already loaded.
 *
 * @lookup_fn - a callback function to be populated with the
 *   lookup function.  The lookup function must throw an exception
 *   if it fails.
 *
 * The library initialization function is C callable exported symbol,
 * but returns a C++ function pointer to the lookup function.
*/
struct library_init_args
{
  std::function<void(const std::string&, lookup_args*)> lookup_fn;
};

/**
 * library_init_fn - type of the library initialization function
 * The name of the library initialization function is fixed to
 * "library_init".
*/
using library_init_fn = void (*)(library_init_args*);

} // cpu

} // namespace xrt
#endif
