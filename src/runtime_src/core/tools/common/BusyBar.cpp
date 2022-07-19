/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// ------ I N C L U D E   F I L E S -------------------------------------------
#include "BusyBar.h"

// Local - Include Files
#include "EscapeCodes.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>

// System - Include Files
#include <iostream>

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBUtilities;

// ------ S T A T I C   V A R I A B L E S -------------------------------------
static unsigned int busy_bar_width = 20;

static boost::format fmt_update(EscapeCodes::cursor().hide()+
                               EscapeCodes::fgcolor::reset()+ "[" +
                               // This formats the width of progress bar and accepts a string
                               // It breaks down into this %-20s which accepts a string that is 20 characters long
                               EscapeCodes::fgcolor(EscapeCodes::FGC_IN_PROGRESS).string() + "%-" + std::to_string(busy_bar_width)+ "s" +
                               EscapeCodes::fgcolor::reset()+ "]" +
                               EscapeCodes::fgcolor(EscapeCodes::FGC_IN_PROGRESS).string() +
                               EscapeCodes::fgcolor::reset()+ ": %s... < %s >\n"
                              );

BusyBar::BusyBar(const std::string &op_name, std::ostream &output)
  : m_op_name(op_name)
  , m_iteration(0)
  , m_is_thread_running(false)
  , m_output(output)
{}

BusyBar::~BusyBar()
{
  // Make sure the thread has been collected and the bar text is removed
  finish();
}

void
BusyBar::start(const bool is_batch)
{
  std::lock_guard lock(m_data_guard);
  if (m_is_thread_running)
    throw std::runtime_error("Timer already running");
  // Reset all data fields
  m_timer.reset();
  m_iteration = 0;
  m_is_thread_running = true;
  // Batch mode does not use escape codes
  if (is_batch)
    m_busy_thread = std::thread([&] { update_batch(); });
  else
    m_busy_thread = std::thread([&] { update(); });
}

void
BusyBar::finish()
{
  // This is an atomic bool that does not need a lock
  if (m_is_thread_running) {
    m_is_thread_running = false;
    m_busy_thread.join();
  }
}

void
BusyBar::check_timeout(const std::chrono::seconds& max_duration)
{
  std::unique_lock lock(m_data_guard);
  if (m_timer.get_elapsed_time() >= max_duration) {
    lock.unlock();
    finish();
    throw std::runtime_error("Time Out");
  }
}

void
BusyBar::update_batch()
{
  m_output << "Running Test: ";
  m_output.flush();
  // Loop until the main thread commands this thread to stop
  while (m_is_thread_running) {
    // Sleep for some time to prevent updating to quickly
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Protext class data
   std::lock_guard lock(m_data_guard);

    // Write the new progress bar
    m_output << ".";

    m_output.flush();
  }
  // Add a newline to seperate the details from the running bar
  m_output << "\n";
  m_output.flush();
}

void
BusyBar::update()
{
  m_output << fmt_update % "" % m_op_name % Timer::format_time(std::chrono::seconds(0));
  m_output.flush();
  // Loop until the main thread commands this thread to stop
  while (m_is_thread_running) {
    // Sleep for some time to prevent updating to quickly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Protect class data
    std::lock_guard lock(m_data_guard);
    // Create the busy bar filled with spaces and a symbol at the end
    const static std::string symbol = "<->";
    auto bar_end = busy_bar_width - symbol.length();
    // Use 2 times the length to handle going backwards!
    auto bar_length = m_iteration % (bar_end * 2);
    // Backwards logic
    if (bar_length > bar_end)
      bar_length = (bar_end * 2) - bar_length;
    std::string busy_bar = std::string(bar_length, ' ');
    busy_bar.append(symbol);

    // Update the time and iteration after printing the first block
    m_iteration++;

    // Write the new progress bar
    m_output << EscapeCodes::cursor().prev_line()
             << fmt_update % busy_bar % m_op_name % Timer::format_time(m_timer.get_elapsed_time());

    m_output.flush();
  }

  // Clear the busy bar text from the command line after joining the thread
  // to clear the busy bar text. Also put the cursor back onto the screen!
  m_output << EscapeCodes::cursor().prev_line() << std::string(80, ' ') << std::endl;
  m_output << EscapeCodes::cursor().prev_line() << EscapeCodes::cursor().show();
  m_output.flush();
}
