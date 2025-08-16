// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SmiWatchMode.h"
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
  std::atomic<bool> watch_interrupted{false};

  void (*old_signal_handler)(int) = nullptr;

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
                               const std::vector<std::string>& elements_filter,
                               std::ostream& output,
                               const ReportGenerator& report_generator,
                               const std::string& report_title)
{
  if (!device || !report_generator) {
    output << "Error: Invalid device or report generator provided to watch mode\n";
    return;
  }

  // Set up signal handler for Ctrl+C
  signal_handler::setup();
  
  output << "Starting " << report_title << " Watch Mode (Press Ctrl+C to exit)\n";
  output << "Update interval: 1 second\n";
  output << "=======================================================\n\n";
  output.flush();
  
  signal_handler::reset_interrupt();
  std::string last_report;
  
  // Filter out watch-specific options for the report generator
  auto filtered_elements = filter_out_watch_options(elements_filter);
  
  while (signal_handler::active()) {
    try {
      // Generate current report
      std::string current_report = report_generator(device, filtered_elements);
      
      // Only update display if content has changed
      if (current_report != last_report) {
        // Clear screen for better readability - ANSI codes work on most modern terminals
        output << "\033[2J\033[H";
        
        output << current_report;
        output << "\n(Press Ctrl+C to exit watch mode | Last update: " << xrt_core::timestamp() << ")";
        output.flush();
        
        last_report = current_report;
      }
    } 
    catch (const std::exception& e) {
      output << "Error generating report: " << e.what() << "\n";
      output.flush();
    }
    
    // Sleep for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  
  output << "\n\nWatch mode interrupted by user.\n";
  
  // Restore original signal handler
  signal_handler::restore();
}

std::vector<std::string> 
smi_watch_mode::
filter_out_watch_options(const std::vector<std::string>& elements_filter)
{
  std::vector<std::string> filtered;
  filtered.reserve(elements_filter.size());
  
  std::copy_if(elements_filter.begin(), elements_filter.end(),
               std::back_inserter(filtered),
               [](const std::string& filter) {
                 return !(filter == "watch" || filter.find("watch=") == 0);
               });
  
  return filtered;
}

