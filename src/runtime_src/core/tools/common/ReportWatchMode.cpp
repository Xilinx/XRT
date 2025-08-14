// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportWatchMode.h"
#include "core/common/time.h"

// 3rd Party Library - Include Files
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <algorithm>

// ------ S T A T I C   V A R I A B L E S -------------------------------------
namespace {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) - Required for signal handling
  std::atomic<bool> g_watch_interrupted{false};

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) - Required for signal handling
  void (*g_old_signal_handler)(int) = nullptr;

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) - Required for signal handling
  bool g_signal_handler_set = false;

void watch_signal_handler(int signal) {
  if (signal == SIGINT) {
    g_watch_interrupted = true;
  }
}

} // unnamed namespace

// ------ F U N C T I O N   D E F I N I T I O N S -------------------------

bool 
report_watch_mode::parse_watch_mode_options(const std::vector<std::string>& elements_filter)
{
  return std::any_of(elements_filter.begin(), elements_filter.end(),
                     [](const std::string& filter) {
                       return filter == "watch";
                     });
}

void 
report_watch_mode::run_watch_mode(const xrt_core::device* device,
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
  setup_signal_handler();
  
  output << "Starting " << report_title << " Watch Mode (Press Ctrl+C to exit)\n";
  output << "Update interval: 1 second\n";
  output << "=======================================================\n\n";
  output.flush();
  
  g_watch_interrupted = false;
  std::string last_report;
  
  // Filter out watch-specific options for the report generator
  auto filtered_elements = filter_out_watch_options(elements_filter);
  
  while (!g_watch_interrupted) {
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
    } catch (const std::exception& e) {
      output << "Error generating report: " << e.what() << "\n";
      output.flush();
    }
    
    // Sleep for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  
  output << "\n\nWatch mode interrupted by user.\n";
  
  // Restore original signal handler
  restore_signal_handler();
}

std::vector<std::string> 
report_watch_mode::filter_out_watch_options(const std::vector<std::string>& elements_filter)
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

void 
report_watch_mode::setup_signal_handler()
{
  if (!g_signal_handler_set) {
    g_old_signal_handler = signal(SIGINT, watch_signal_handler);
    g_signal_handler_set = true;
  }
}

void 
report_watch_mode::restore_signal_handler()
{
  if (g_signal_handler_set) {
    static_cast<void>(signal(SIGINT, g_old_signal_handler));
    g_signal_handler_set = false;
  }
}

