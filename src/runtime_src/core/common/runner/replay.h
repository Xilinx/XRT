// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_COMMON_RUNNER_REPLAY_H_
#define XRT_COMMON_RUNNER_REPLAY_H_

#include "xrt/detail/config.h"
#include "xrt/detail/pimpl.h"
#include "xrt/experimental/xrt_exception.h"

#include "repo.h"

#include <filesystem>
#include <string>

namespace xrt {
class device;
}

namespace xrt_core {

/**
 * class replay - Replay a captured XRT application execution
 *
 * Replays a capture script (replay.json) with associated artifacts.
 * The replay executes frames in the same threading pattern as the
 * original application.
 *
 * Example usage:
 * @code
 *   xrt::device device{0};
 *   xrt_core::replay rply{device, "replay.json", "/tmp/artifacts"};
 *   rply.execute();
 *   auto report = rply.get_report();
 * @endcode
 */
class replay_impl;
class replay : public xrt::detail::pimpl<replay_impl>
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

public:
  /**
   * artifacts_repository - A map of artifacts
   *
   * The replay can be constructed with an artifacts repository, in
   * which case artifacts referenced by the replay script are looked
   * up in the repository rather than from disk.
   *
   * Note, that copies of the artifact repository share the same
   * underlying implementation.
   */
  using artifacts_repository = xrt_core::artifacts::repository;

  replay() = default;

  /**
   * ctor - Create replay from script path and artifacts directory
   *
   * @param device - XRT device to use for replay
   * @param script - Path to replay.json file
   * @param artifacts_dir - Directory containing artifact files
   *                        (xclbins, elfs, buffer data)
   *
   * Loads replay.json and creates resources needed for execution.
   * Artifacts referenced by the script are loaded from artifacts_dir.
   */
  XRT_API_EXPORT
  replay(const xrt::device& device, const std::string& script,
         const std::filesystem::path& artifacts_dir);

  /**
   * ctor - Create replay from script path and artifacts repository
   *
   * @param device - XRT device to use for replay
   * @param script - Path to replay.json file
   * @param repo - Artifacts repository with preloaded artifacts
   *
   * Loads replay.json and creates resources needed for execution.
   * Artifacts referenced by the script are loaded from the repository.
   */
  XRT_API_EXPORT
  replay(const xrt::device& device, const std::string& script,
         const artifacts_repository& repo);

  explicit
  operator bool() const
  {
    return handle != nullptr;
  }

  /**
   * execute() - Execute the replay
   *
   * Replays all frames from the capture script. Worker threads are
   * created to match the application's threading pattern from capture.
   * Returns when all frames have completed execution.
   *
   * This function can only be called once per replay object.
   *
   * @throws replay::error if execution fails
   */
  XRT_API_EXPORT
  void
  execute();

  /**
   * get_report() - Get replay execution report as JSON string
   *
   * Returns execution metrics in JSON format:
   * {
   *   "cpu": {"elapsed_us": <microseconds>},
   *   "iterations": 1,
   *   "frames": <count>
   * }
   *
   * @return JSON string with execution metrics
   */
  XRT_API_EXPORT
  std::string
  get_report() const;
};

} // namespace xrt_core

#endif
