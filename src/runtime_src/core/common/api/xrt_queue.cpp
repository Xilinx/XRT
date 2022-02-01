/*
 * Copyright (C) 2022, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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
// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_queue.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_queue.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_queue.h"

#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#ifdef _WIN32
# pragma warning( disable : 4244 )
#endif

namespace xrt {

// class queue_impl - insulated implemention of an xrt::queue
//
// Manages and executes enqueued tasks.
// Tasks are executed and complete in order of enqueuing.
//
// An queue is associated with exactly one handler thread that executes
// the task asynchronously to the enqueuer.
class queue_impl
{
  std::queue<xrt::queue::task> m_queue;  // task queue
  std::mutex m_mutex;
  std::condition_variable m_work;
  bool m_stop = false;

  // single worker threads to run the tasks
  std::thread m_worker;

  // worker thread, executes tasks as they become ready
  void
  run()
  {
    while (!m_stop) {
      xrt::queue::task task;

      // exclusive synchronized region
      {
        std::unique_lock lk(m_mutex);
        m_work.wait(lk, [this] { return m_stop || !m_queue.empty(); });

        if (m_stop)
          return;

        task = std::move(m_queue.front());
        m_queue.pop();
      }

      // allow enqueue while executing
      task.execute();
    }
  }

public:
  queue_impl()
    : m_worker({ [this] { run(); } });
  {}

  // Shut down worker thread
  ~queue_impl()
  {
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      m_stop = true;
      m_work.notify_one();
    }
    m_worker.join();
  }

  // Enqueue a task and notify worker
  void
  enqueue(queue::task&& t)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_queue.push(std::move(t));
    m_work.notify_one();
  }
};

} // xrt

////////////////////////////////////////////////////////////////
// xrt_enqueue C++ API implmentations (xrt_enqueue.h)
////////////////////////////////////////////////////////////////
namespace xrt {

queue::
queue()
  : m_impl(std::make_shared<queue_impl>())
{}

void
queue::
add_task(task&& t)
{
  m_impl->enqueue(std::move(t));
}

} // xrt
