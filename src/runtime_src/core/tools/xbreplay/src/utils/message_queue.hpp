// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "utils/message.hpp"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <memory>
#include <thread>
#include <queue>

namespace xrt_core::tools::xbreplay::utils {

class message_queue
{
  public:
  /*
   * This function is used to push message to the queue.
   */
  void send (std::shared_ptr<message> msg)
  {
    std::lock_guard lock(m_mutex);
    m_message.push(msg);
    m_condition.notify_one(); // Notify receiver that a new message is available
  }

  /*
   * This funciton is used receive message from the queue.
   */
  std::shared_ptr<message> receive()
  {
    std::unique_lock lock(m_mutex);
    m_condition.wait(lock, [this] { return !m_message.empty(); });

    auto msg = m_message.front();
    m_message.pop();
    return msg;
  }

  private:
  std::queue<std::shared_ptr<message>> m_message;
  std::mutex m_mutex;
  std::condition_variable m_condition;
};

}
