// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

// Please keep external include file dependencies to a minimum
#include <vector>
#include <string>
#include <functional>
#include <iostream>

namespace xrt_core {
  class device;
}

/**
 * @brief Generic watch mode utility for XRT-SMI reports
 * 
 * This utility provides common watch mode functionality that can be used
 * by any XRT-SMI report. It handles:
 * - Element filter parsing for watch mode options
 * - Signal handling (Ctrl+C interruption) with graceful cleanup
 * - Screen clearing with ANSI escape codes for real-time updates
 * - Timing and interval management (1-second intervals)
 * - Cross-platform compatibility (Windows/POSIX)
 * 
 * Usage Example:
 * @code
 * if (report_watch_mode::parse_watch_mode_options(elements_filter)) {
 *   auto generator = [](const xrt_core::device* dev, const std::vector<std::string>& filters) {
 *     return my_report_function(dev, filters);
 *   };
 *   report_watch_mode::run_watch_mode(device, elements_filter, std::cout, generator, "My Report");
 * }
 * @endcode
 */
class report_watch_mode {
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
  using ReportGenerator = std::function<std::string(const xrt_core::device*, const std::vector<std::string>&)>;

  /**
   * @brief Parse watch mode options from element filters
   * 
   * @param elements_filter Vector of element filter strings to parse
   * @return true if watch mode is requested, false otherwise
   * 
   * Supported formats:
   * - "watch" - Enable watch mode with default 1 second interval
   * 
   * Future extensions could support:
   * - "watch=<seconds>" - Custom interval (not implemented yet)
   * 
   * @note This function only checks for watch mode presence, 
   *       it does not validate or parse other filter options
   */
  static bool parse_watch_mode_options(const std::vector<std::string>& elements_filter);

  /**
   * @brief Run watch mode with the provided report generator
   * 
   * @param device The XRT device to query for real-time data
   * @param elements_filter Element filters (watch-specific filters will be filtered out automatically)
   * @param output Output stream for the report (typically std::cout)
   * @param report_generator Function to generate report content for each iteration
   * @param report_title Title to display in watch mode header (e.g., "Context Health")
   * 
   * This function implements the complete watch mode workflow:
   * - Sets up SIGINT (Ctrl+C) signal handling for graceful interruption
   * - Runs an infinite loop with 1-second intervals until interrupted
   * - Clears screen using ANSI escape codes for real-time updates
   * - Only updates display when report content actually changes (efficiency)
   * - Shows timestamp using XRT's native timestamp format (GMT)
   * - Restores original signal handler on exit
   * - Handles all exceptions internally with error reporting
   * 
   * @note This function blocks until user interrupts with Ctrl+C
   * @note Thread-safe signal handling using atomic variables
   */
  static void run_watch_mode(const xrt_core::device* device,
                            const std::vector<std::string>& elements_filter,
                            std::ostream& output,
                            const ReportGenerator& report_generator,
                            const std::string& report_title = "Report");

  /**
   * @brief Filter out watch-specific options from element filters
   * 
   * @param elements_filter Input element filters that may contain watch options
   * @return New vector with watch-specific options removed
   * 
   * Removes the following patterns:
   * - "watch" - Simple watch mode activation
   * - "watch=<value>" - Watch mode with custom options (future extension)
   * 
   * This allows the filtered list to be passed to report generators
   * without them needing to handle watch-specific syntax.
   * 
   * @note The returned vector may be smaller than the input
   * @note Original vector is not modified (returns new vector)
   */
  static std::vector<std::string> filter_out_watch_options(const std::vector<std::string>& elements_filter);

private:
  /**
   * @brief Set up SIGINT signal handler for watch mode interruption
   * 
   * - Saves the current SIGINT handler for restoration later
   * - Installs custom handler that sets atomic interrupt flag
   * - Uses thread-safe signal handling compatible with XRT patterns
   * - Only installs once (subsequent calls are ignored)
   * 
   * @note Must be paired with restore_signal_handler() call
   * @note Uses standard signal() function for cross-platform compatibility
   */
  static void setup_signal_handler();

  /**
   * @brief Restore the original SIGINT signal handler
   * 
   * - Restores the signal handler that was active before setup_signal_handler()
   * - Clears internal state flags
   * - Safe to call multiple times or without prior setup
   * 
   * @note Should be called before exiting watch mode
   */
  static void restore_signal_handler();
};
