// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "core/common/query_requests.h"

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace xrt_core {
class device;
namespace query {
struct firmware_debug_buffer;
}
}

//This is arbitrary for the moment. We can change this once we do real testing with firmware data
constexpr size_t debug_buffer_size = static_cast<size_t>(4) * 1024 * 1024; // 4MB // NOLINT(readability-magic-numbers)

/**
 * @brief RAII wrapper for firmware debug buffer management
 * 
 * Encapsulates buffer allocation and firmware_debug_buffer configuration
 * with automatic memory management and proper RAII semantics.
 */
class smi_debug_buffer {
private:
  std::vector<char> buffer;
  xrt_core::query::firmware_debug_buffer log_buffer;

public:
  explicit 
  smi_debug_buffer(uint64_t abs_offset = 0, 
                   bool b_wait = false, 
                   size_t size = debug_buffer_size);

  xrt_core::query::firmware_debug_buffer& 
  get_log_buffer() 
  { 
    return log_buffer; 
  }

  uint64_t 
  get_size() const
  {
    return log_buffer.size;
  }

  void*
  get_data() const
  {
    return log_buffer.data;
  }

  uint64_t
  get_offset() const
  {
    return log_buffer.abs_offset;
  }
};

/**
 * @brief Generic watch mode utility for XRT-SMI reports
 * 
 * This utility provides common watch mode functionality that can be used
 * by any XRT-SMI report. It handles:
 * - Element filter parsing for watch mode options
 * - Signal handling (Ctrl+C interruption) with graceful cleanup
 * - Optional terminal clear between updates (examine --watch only)
 * - Optional refresh interval (seconds; 0 = no delay between updates)
 * - Cross-platform compatibility (Windows/POSIX)
 * 
 * Usage Example:
 * @code
 * smi_watch_mode::run_watch_mode(device, std::cout, report_generator);           // no delay
 * smi_watch_mode::run_watch_mode(device, std::cout, report_generator, interval);  // seconds between updates
 * @endcode
 */
class smi_watch_mode {
public:
  /**
   * @brief Function type for generating report content
   * 
   * @param device The XRT device to query for data
   * @param elements_filter The element filters (excluding watch-specific ones)
   * @return The formatted report content as a string
   * 
   * The generator function should:
   * - Query the device for current data
   * - Apply any filtering based on elements_filter
   * - Return formatted string ready for display
   * - Handle any exceptions internally (return error message if needed)
   */
  using ReportGenerator = std::function<std::string(const xrt_core::device*)>;

  /**
   * @brief Parse watch mode options from element filters
   * 
   * @param elements_filter Vector of element filter strings to parse
   * @return true if watch mode is requested, false otherwise
   * 
   * Supported formats:
   * - "watch" - Enable watch mode
   * 
   * @note This function only checks for watch mode presence, 
   *       it does not validate or parse other filter options
   */
  static bool 
  parse_watch_mode_options(const std::vector<std::string>& elements_filter);

  /**
   * @brief Run watch mode with the provided report generator
   *
   * @param device May be nullptr for host-only report generators.
   * @param output Output stream for the report (typically std::cout)
   * @param report_generator Function to generate report content for each iteration
   * @param refresh_interval_seconds Seconds to sleep after each update before the next (default 0 = no sleep)
   * @param refresh_terminal If true, clear the terminal before each update after the first (examine reports only).
   *
   * @note Blocks until user interrupts with Ctrl+C
   */
  static void 
  run_watch_mode(const xrt_core::device* device,
                 std::ostream& output,
                 const ReportGenerator& report_generator,
                 unsigned refresh_interval_seconds = 0,
                 bool refresh_terminal = false);
};
