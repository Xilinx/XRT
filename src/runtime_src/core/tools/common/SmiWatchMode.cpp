// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SmiWatchMode.h"
#include "core/common/query_requests.h"
#include "core/common/time.h"

// 3rd Party Library - Include Files
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <sstream>
#include <thread>

// ------ S T A T I C   V A R I A B L E S -------------------------------------
namespace signal_handler {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) - Required for signal handling
  std::atomic<bool> watch_interrupted{false};

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) - Required for signal handling
  void (*old_signal_handler)(int) = nullptr;

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) - Required for signal handling
  bool signal_handler_set = false;

  void watch(int signal) {
    if (signal == SIGINT) {
      watch_interrupted = true;
    }
  }

  /**
   * @brief Set up SIGINT signal handler for watch mode interruption
   * 
   * - Saves the current SIGINT handler for restoration later
   * - Installs custom handler that sets atomic interrupt flag
   * - Uses thread-safe signal handling 
   * 
   * @note Must be paired with restore() call
   * @note Uses standard signal() function for cross-platform compatibility
   */
  void setup() {
    if (!signal_handler_set) {
      old_signal_handler = signal(SIGINT, watch);
      signal_handler_set = true;
    }
  }

  /**
   * @brief Restore the original SIGINT signal handler
   * 
   * - Restores the signal handler that was active before setup()
   * - Clears internal state flags
   * - Safe to call multiple times or without prior setup
   * 
   * @note Should be called before exiting watch mode
   */
  void restore() {
    if (signal_handler_set) {
      static_cast<void>(signal(SIGINT, old_signal_handler));
      signal_handler_set = false;
    }
  }

  /**
   * @brief Reset the interrupt flag to allow new watch mode session
   * 
   * This method provides controlled access to reset the interrupt state
   * without exposing the internal variable directly.
   */
  void reset_interrupt() {
    watch_interrupted = false;
  }

  /**
   * @brief Check if watch mode is still active
   * 
   * @return true if watch mode should continue, false if interrupted
   * 
   * This method provides controlled read access to the interrupt state
   * with inverted logic for more intuitive usage in while loops.
   */
  bool active() {
    return !watch_interrupted;
  }

} // namespace signal_handler

bool 
smi_watch_mode::
parse_watch_mode_options(const std::vector<std::string>& elements_filter)
{
  return std::any_of(elements_filter.begin(), elements_filter.end(),
                     [](const std::string& filter) {
                       return filter == "watch";
                     });
}

void 
smi_watch_mode::
run_watch_mode(const xrt_core::device* device,
               std::ostream& output,
               const ReportGenerator& report_generator)
{
  if (!device || !report_generator) {
    output << "Error: Invalid device or report generator provided to watch mode\n";
    return;
  }

  // Set up signal handler for Ctrl+C
  signal_handler::setup();
  
  signal_handler::reset_interrupt();
  std::string last_report;
  
  while (signal_handler::active()) {
    try {
      // Generate current report
      std::string current_report = report_generator(device);
      
      // Only update display if content has changed
      if (current_report != last_report) {
        output << current_report;
        output.flush();
        last_report = current_report;
      }
    } 
    catch (const std::exception& e) {
      output << "Error generating report: " << e.what() << "\n";
      output.flush();
    }
  }
  
  output << "\n\nWatch mode interrupted by user.\n";
  
  // Restore original signal handler
  signal_handler::restore();
}

smi_debug_buffer::
smi_debug_buffer(uint64_t abs_offset, bool b_wait, size_t size)
  : buffer(size),
    log_buffer{abs_offset, buffer.data(), size, b_wait}
{}
