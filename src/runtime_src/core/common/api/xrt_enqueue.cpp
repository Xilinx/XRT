/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
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
// core/include/experimental/xrt_enqueue.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_enqueue.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_enqueue.h"

#include "core/common/debug.h"

#include <memory>
#include <vector>
#include <queue>
#include <unordered_set>
#include <set>
#include <functional>
#include <algorithm>
#include <thread>
#include <mutex>

#ifdef _WIN32
# pragma warning( disable : 4244 )
#endif

namespace {

}

namespace xrt {

// class event_impl - insulated implementation of an xrt::event
//
// Objects of event_impl are attached to asynchronous waitable
// objects, e.g. kernel run objects such that the event can be
// notified upon completion of the asynchronous operation.
// 
// Objects of event_impl are inserted into an event_queue_impl
// which participates in the ownership of the event.  The event
// is removed from the queue when it is complete.
//
// An enqueued run object holds a reference to an event, which only
// goes away once the run object is deleted.
class event_impl : public std::enable_shared_from_this<event_impl>
{
  std::mutex m_mutex;
  event_queue::task m_task;
  event_queue_impl* m_event_queue;
  std::vector<event_impl*> m_chain;
  unsigned int m_wait_count;
  unsigned int m_uid = 0;
  bool m_done = false;

public:
  // Chain this event to argument event.  This increments
  // the wait count on the argument event, which cannot
  // proceed to execute before this event has completed.
  void
  chain(event_impl* ev)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_done)
      return;
    m_chain.push_back(ev);
    ++ev->m_wait_count;
  }

  // Try to submit this event for execution.  This function
  // decrements the wait_count and if zero, submits the event
  // for execution through its associated event queue, where
  // it will be picked up by an event handler and executed.
  //
  // Return: true if wait_count is zero and event was submitted
  // for execution, false otherwise
  bool
  submit();

  // Try to submit this event for execution and associate it
  // with argument event queue.
  //
  // This function is called when the event is enqueued on the
  // event queue.  
  //
  // This event is removed from the event queue when it completes.
  bool
  submit(event_queue_impl* evq)
  {
    m_event_queue = evq;
    return submit();
  }

  // Execute this event.
  //
  // Function is called by an event handler that wants to
  // execute this event.
  void
  execute()
  {
    XRT_DEBUGF("event_impl::execute(%d)\n", m_uid);
    m_task.execute(get_shared_from_this());
  }

  // Mark this event complete.
  //
  // For synchronous event operations, this function is called by the
  // task associated with the event once the task is complete.
  //
  // For asynchronous event operations, this function is called once
  // the asynchronous operation completes, which is after the
  // associated task has executed.  For example, the task may be a
  // kernel function which when started returns a run object.  This
  // function (done()) is called when the run object completes.
  void
  done();

  // Allow clients of event_impl* to retrieve the associated
  // shared_ptr that was created when the event_impl object was
  // constructed. This makes it possible to participate in ownership
  // of the event_impl even when just holding a raw pointer.
  std::shared_ptr<xrt::event_impl>
  get_shared_from_this()
  {
    return shared_from_this();
  }

public:
  // Construct the event implementation with a task and depedencies
  // that must complete before the task can be executed.
  //
  // By default the wait count of event is (1), immediately upon
  // enqueing, the event queue will try to submit the event thus
  // decrement the default wait count.
  //
  // The wait count of this event is incremented per number of active
  // dependencies, this forming an event graph.
  event_impl(event_queue::task&& t, const std::vector<event_queue::event>& deps)
    : m_task(std::move(t))
    , m_wait_count(1)
  {
    static unsigned int count = 0;
    m_uid = count++;
    XRT_DEBUGF("event_impl::event_impl(%d)\n", m_uid);
    for (auto& ev : deps)
      if (auto& impl = ev.get_impl())
        impl->chain(this);
  }

  // Event destructor added for debuggability.
  ~event_impl()
  {
    XRT_DEBUGF("event_impl::~event_impl(%d)\n", m_uid);
  }
};

// class event_queue_impl - insulated implemention of an xrt::event_queue
//
// Manages enqueued tasks in form of events that form an event graph
// based on dependencies between the events.
//
// When an event is enqueued it is added to the set of events to
// retain ownership of the event. As part of enqueuing the event, the
// event is associated with the event queue by attempting to submit
// it.
//
// If all event dependencies have been satisfied, the event moves to
// submitted state where it added the event queues task queue.  The
// task queue is serviced by one or more event handlers that execute
// the events in first-in-first-out order.
//
// An event queue is associated with one or more event handlers, which
// participate in ownership of the queue.
class event_queue_impl
{
  // Event comparator for heterogenous lookup of events
  struct event_cmp
  {
    using event_ptr = std::shared_ptr<event_impl>;
    using is_transparent = void;
    bool operator() (const event_ptr& lhs, const event_ptr& rhs) const { return lhs < rhs; }
    bool operator() (const event_ptr& lhs, const event_impl* rhs) const { return lhs.get() < rhs; }
    bool operator() (const event_impl* lhs, const event_ptr& rhs) const { return lhs < rhs.get(); }
  };
  
  std::queue<event_impl*> m_queue;                           // task queue
  std::set<std::shared_ptr<event_impl>, event_cmp> m_events; // enqueued events
  std::mutex m_mutex;
  std::condition_variable m_work;

public:
  // Enqueue an event and try submit it.
  void
  enqueue(const std::shared_ptr<event_impl>& event)
  {
    {
      std::lock_guard<std::mutex> lk(m_mutex);
      m_events.insert(event);
    }
    event->submit(this);
  }

  // Submit argument event by inserting it in the queue that is
  // serviced by event handlers.  Notify the handlers that work
  // is ready.
  void
  submit(event_impl* ev)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_queue.push(ev);
    m_work.notify_one();
  }

  // Upon completion, the event is removed from the ownership
  // retaining set, which effectively deletes the event if no
  // other objects participate in the events ownership.
  void
  remove(event_impl* ev)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto itr = m_events.find(ev);
    if (itr != m_events.end())
      m_events.erase(itr);
  }

  // Notify any waiting for work from this queue. Used by event
  // handler destructor to force termination of the event handler
  // thread which is waiting for work.
  void
  notify()
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_work.notify_all();
  }

  // Get work from the queue.  This function is used by
  // event handlers to wait for event that are ready to
  // be executed.  The function pops of events from the
  // front of the queue.
  event_impl*
  get_work()
  {
    std::unique_lock<std::mutex> lk(m_mutex);
    // notify() need to be able to wake up handlers waiting
    // for tasks, so wait is deliberately not using 'while'
    if (m_queue.empty())
      m_work.wait(lk);

    // return null task if no real ones
    if (m_queue.empty())
      return nullptr;

    // first in first out
    auto e = std::move(m_queue.front());
    m_queue.pop();
    return e;
  }
};
  
// See comment block in event::impl::submit() declaration.
bool
event_impl::
submit()
{
  std::lock_guard<std::mutex> lk(m_mutex);
  if (--m_wait_count)
    return false;
  
  m_event_queue->submit(this);
  return true;
}
// See comment block in event::impl::done() declaration.
void
event_impl::
done()
{
  // assert(m_mutex is locked)
  XRT_DEBUGF("event_impl::done(%d)\n", m_uid);
  m_done = true;
  for (auto& ev : m_chain)
    ev->submit();

  m_event_queue->remove(this);
}

// class event_handler_impl - insulated implementation of xrt::event_handler
//
// An event handler is a consumer of events that are ready to
// be executed.
//
// The handler is associated with exactly one event queue and shares
// ownership of this event queue.  Upon destruction of the event
// handler, the event queue is requested to wake up the waiting thread
// routine, which then gracefully exits.
class event_handler_impl
{
  std::atomic<bool> m_stop {false};
  std::thread m_handler;
  event_queue m_retain;            // retain ownership of event queue
  event_queue_impl* m_event_queue; // convienience

  // Thread run routine that consumes and executes events
  // that are ready to be executed.
  void
  run()
  {
    while (!m_stop)
      if (auto e = m_event_queue->get_work())
        e->execute();
  }
  
public:
  // Construct event handler and retain ownership of event queue
  event_handler_impl(const event_queue& q)
    : m_retain(q)
    , m_event_queue(q.get_impl())
  {
    m_handler = std::thread(&event_handler_impl::run, this);
  }

  // Destruct event handler requesting event queue to notify waiting
  // workers.  While all workers are notified, only the ones that are
  // being stopped will actually exit so while an event queue can have
  // multiple handlers, handlers can exit one by one.  When last handler
  // is destructed the retained event queue is effectively deleted if
  // noone else shares ownership.
  ~event_handler_impl()
  {
    m_stop = true;
    m_event_queue->notify();
    m_handler.join();
  }
};

} // xrt

////////////////////////////////////////////////////////////////
// xrt_enqueue implementation of extension APIs not exposed to end-user
////////////////////////////////////////////////////////////////
namespace xrt_core { namespace enqueue {

// This is a helper function exposed to internal XRT clients
// Asynchronous execution need access to notify events when the
// asynchronous operation is complete.  In order for that to happen an
// event is added to the asynchronous waitable object (for example an
// xrt::run object), which then calls this function when the waitable
// completes.
void
done(xrt::event_impl* ev)
{
  ev->done();
}

}} // namespace enqueue, xrt_core

////////////////////////////////////////////////////////////////
// xrt_enqueue C++ API implmentations (xrt_enqueue.h)
////////////////////////////////////////////////////////////////
namespace xrt {

event_queue::
event_queue()
  : m_impl(std::make_shared<event_queue_impl>())
{}

void
event_queue::
add_event(const event& ev)
{
  m_impl->enqueue(ev.get_impl());
}

event_queue::
event::
event(task&& t, const std::vector<event>& deps)
  : m_impl(std::make_shared<event_impl>(std::move(t), deps))
{}

void
event_queue::
event::
notify(event_impl* impl)
{
  impl->done();
}

event_handler::
event_handler(const event_queue& q)
  : m_impl(std::make_shared<event_handler_impl>(q))
{
}
  
} // xrt
