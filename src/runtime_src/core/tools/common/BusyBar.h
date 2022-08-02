/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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
#ifndef BUSY_BAR_H
#define BUSY_BAR_H

// Include files
#include "XBUtilities.h"

// Please keep these to the bare minimum
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

// ------ N A M E S P A C E ---------------------------------------------------

namespace XBUtilities {

class BusyBar {
 public:
  BusyBar(const std::string &op_name, std::ostream &output);

  void
  start(const bool is_batch);

  void
  finish();

  void
  check_timeout(const std::chrono::seconds& max_duration);

  ~BusyBar();
  BusyBar() = delete;

 private:
  std::string m_op_name;
  unsigned int m_iteration;
  std::mutex m_data_guard;
  std::atomic_bool m_is_thread_running;
  std::ostream &m_output;
  Timer m_timer;
  std::thread m_busy_thread;

  void
  update();

  void
  update_batch();
};
}

#endif
