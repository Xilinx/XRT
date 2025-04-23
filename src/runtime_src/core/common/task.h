/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef xrt_core_common_task_h_
#define xrt_core_common_task_h_

#include "time.h"
#include "debug.h"
#include "config_reader.h"

#include <future>
#include <functional>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>

#ifdef _WIN32
# pragma warning( push )
# pragma warning( disable : 4459 )
#endif

namespace xrt_core { namespace task {

/**
 * Type erased std::packaged_task<RT()>
 *
 * Wraps a std::packaged_task of any return type, such that the task's
 * return value can be captured in a future.
 *
 * Objects of this task class can be stored in any STL container even
 * when the underlying std::packaged_tasks are of different types.
 */
class task
{
  struct task_iholder
  {
    virtual ~task_iholder() {};
    virtual void execute() = 0;
  };

  template <typename Callable>
  struct task_holder : public task_iholder
  {
    Callable held;
    task_holder(Callable&& t) : held(std::move(t)) {}
    void execute() { held(); }
  };

  std::unique_ptr<task_iholder> content;

public:
  task()
    : content(nullptr)
  {}

  task(task&& rhs)
    : content(std::move(rhs.content))
  {}

  template <typename Callable>
  task(Callable&& c)
    : content(new task_holder<Callable>(std::forward<Callable>(c)))
  {}

  task&
  operator=(task&& rhs)
  {
    content = std::move(rhs.content);
    return *this;
  }

  bool
  valid() const
  {
    return content!=nullptr;
  }

  void
  execute()
  {
    content->execute();
  }

  void
  operator() ()
  {
    execute();
  }
};

/**
 * Multiple producer / multiple consumer queue of task objects
 *
 * This code is not specifically tied to task::task, but we keep
 * the defintion here to make task.h stand-alone
 */
template <typename Task>
class mpmcqueue
{
  std::queue<Task> m_tasks;
  mutable std::mutex m_mutex;
  std::condition_variable m_work;
  bool m_stop = false;
  unsigned long long tp = 0;       // time point when last task consumed
  unsigned long long waittime = 0; // wait time from tp to next task avail
  bool debug = false;
public:
  mpmcqueue()
  {}

  explicit mpmcqueue(bool dbg)
    : debug(dbg)
  {}

  void
  addWork(Task&& t)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_tasks.push(std::move(t));
    if (debug && tp) {
      auto wt = time_ns() - tp;
      waittime += wt;
      XRT_DEBUG(std::cout,"m_tasks.size()=",m_tasks.size()," waittime (ms): ",wt*1e-6,"\n");
      tp = 0;
    }
    //XRT_PRINT(std::cout,"m_tasks.size()=",m_tasks.size(),"\n");
    m_work.notify_one();
  }

  Task
  getWork()
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (!m_stop && m_tasks.empty()) {
      m_work.wait(lk);
    }

    Task task;
    if (!m_stop) {
      task = std::move(m_tasks.front());
      m_tasks.pop();
      if (debug && m_tasks.size()==0)
        tp = time_ns();

    }
    return task;
  }

  size_t
  size() const
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_tasks.size();
  }

  void
  stop()
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_stop=true;
    m_work.notify_all();
    if (debug && waittime)
      XRT_PRINT(std::cout,"task queue waittime (ms): ",waittime*1e-6,"\n");
  }
};

/**
 * Specialize for pointer type.  Not specifically a task queue
 * but keep it in this file to make task.h stand-alone
 */
template <typename Task>
class mpmcqueue<Task*>
{
  std::queue<Task*> m_tasks;
  mutable std::mutex m_mutex;
  std::condition_variable m_work;
  bool m_stop;
public:
  mpmcqueue() : m_stop(false) {}

  void
  addWork(Task* t)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_tasks.push(t);
    m_work.notify_one();
  }

  Task*
  getWork()
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    while (!m_stop && m_tasks.empty()) {
      m_work.wait(lk);
    }

    Task* task = nullptr;
    if (!m_stop) {
      task = m_tasks.front();
      m_tasks.pop();
    }
    return task;
  }

  size_t
  size() const
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_tasks.size();
  }

  void
  stop()
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_stop=true;
    m_work.notify_all();
  }
};

using queue = mpmcqueue<task>;

/**
 * event class wraps std::future<RT>
 *
 * Adds a ready() function that can be used to poll if event is ready.
 * Otherwise, currently adds no value compared to bare std::future
 */
template <typename RT>
class event
{
public:
  typedef RT value_type;
  typedef std::future<value_type> FutureType;

private:
  mutable FutureType m_future;

public:
  event() = delete;
  event(const event& rhs) = delete;

  event(const event&& rhs)
    : m_future(std::move(rhs.m_future))
  {}

  event(FutureType&& f)
    : m_future(std::forward<FutureType>(f))
  {}

  event&
  operator=(event&& rhs)
  {
    m_future = std::move(rhs.m_future);
    return *this;
  }

  RT
  wait() const
  {
    return m_future.get();
  }

  RT
  get() const
  {
    return m_future.get();
  }

  bool
  ready() const
  {
    return (m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
  }
};

/**
 * Functions for adding work (tasks) to a task queue.
 *
 * All functions return a task::event that encapsulates the
 * return type of the task.
 *
 * Variants of the functions supports adding both free functions
 * and member functions associated with some class object.
 */
// Free function, lambda, functor

#ifdef __GNUC__
# pragma GCC diagnostic push
# if __GNUC__  >= 7
#  pragma GCC diagnostic ignored "-Wnoexcept-type"
# endif
#endif
template <typename Q,typename F, typename ...Args>
auto
createF(Q& q, F&& f, Args&&... args)
  -> event<decltype(f(std::forward<Args>(args)...))>
{
  typedef decltype(f(std::forward<Args>(args)...)) value_type;
  typedef std::packaged_task<value_type()> task_type;
  task_type t(std::bind(std::forward<F>(f),std::forward<Args>(args)...));
  event<value_type> e(t.get_future());
  q.addWork(std::move(t));
  return e;
}

// Member function.  gcc4.8.4+ does not need the std::bind hints in decltype
// The return type can be deduced the same as the non member function version.
template <typename Q,typename F, typename C, typename ...Args>
auto
createM(Q& q, F&& f, C& c, Args&&... args)
  -> event<decltype(std::bind(std::forward<F>(f),std::ref(c),std::forward<Args>(args)...)())>
{
  typedef decltype(std::bind(std::forward<F>(f),std::ref(c),std::forward<Args>(args)...)()) value_type;
  typedef std::packaged_task<value_type()> task_type;
  task_type t(std::bind(std::forward<F>(f),std::ref(c),std::forward<Args>(args)...));
  event<value_type> e(t.get_future());
  q.addWork(std::move(t));
  return e;
}
#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

// A task worker is a thread function getting work off a task queue.
// The worker runs until the queue is stopped.
inline void
worker_debug(queue& q,const std::string& id)
{
  unsigned long loops = 0;
  unsigned long long worktime = 0;
  unsigned long long waittime = 0;
  while (true) {
    ++loops;
    auto timepoint = time_ns();
    auto t = q.getWork();
    if (!t.valid())
      break;
    waittime += time_ns() - timepoint;
    t();
    worktime += time_ns() - timepoint;

    // don't count time from program start to first job
    if (loops==1) {
      worktime -= waittime;
      waittime = 0;
    }
  }

  worktime -= waittime;
  XRT_PRINT(std::cout,"task worker (",id,")"
            ,", loops: ",loops
            ,", worktime (ms): ",worktime*1e-6
            ,", waitime (ms): ",waittime*1e-6,"\n");
}

inline void
worker_ndebug(queue& q)
{
  while (true) {
    auto t = q.getWork();
    if (!t.valid())
      break;
    t();
  }
}

inline void
worker2(queue& q, const std::string& id="")
{
  if (xrt_core::config::get_xrt_debug())
    return worker_debug(q,id);
  else
    return worker_ndebug(q);
}

inline void
worker(queue& q)
{
  return worker2(q,"");
}
}} // task,xrt_core

#ifdef _WIN32
# pragma warning( pop )
#endif

#endif
