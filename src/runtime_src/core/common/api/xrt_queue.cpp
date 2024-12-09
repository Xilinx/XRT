// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_queue.h
#define XRT_API_SOURCE         // exporting xrt_queue.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/xrt/experimental/xrt_queue.h"

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
// Tasks are executed and completed in order of enqueuing.
//
// A queue is associated with exactly one handler thread that executes
// the task asynchronously to the enqueuer.
class queue_impl
{
  std::queue<xrt::queue::task> m_queue;  // task queue
  std::mutex m_mutex;
  std::condition_variable m_work;
  bool m_stop = false;

  // single worker thread to run the tasks
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
    : m_worker([this] { run(); })
  {}

  // Shut down worker thread
  ~queue_impl()
  {
    {
      std::lock_guard lk(m_mutex);
      m_stop = true;
      m_work.notify_one();
    }
    m_worker.join();
  }

  queue_impl(const queue_impl&) = delete;
  queue_impl(queue_impl&&) = delete;
  queue_impl& operator=(const queue_impl&) = delete;
  queue_impl& operator=(queue_impl&&) = delete;

  // Enqueue a task and notify worker
  void
  enqueue(queue::task&& t)
  {
    std::lock_guard lk(m_mutex);
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
