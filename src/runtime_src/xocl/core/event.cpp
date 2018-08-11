/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "event.h"
#include "command_queue.h"
#include "context.h"

#include "xrt/config.h"
#include "xrt/util/task.h"
#include "xrt/util/memory.h"

#include "xocl/api/plugin/xdp/profile.h"

#include <iostream>
#include <cassert>

namespace {

XOCL_UNUSED
static std::string
to_string(cl_int status)
{
  switch (status) {
  case CL_QUEUED:
    return "queued";
  case CL_SUBMITTED:
    return "submitted";
  case CL_RUNNING:
    return "running";
  case CL_COMPLETE:
    return "complete";
  case -1:
    return "new";
  }
  return "???";
}

static xocl::event::event_callback_list sg_constructor_callbacks;
static xocl::event::event_callback_list sg_destructor_callbacks;
} // namespace

namespace xocl {

event::
event(command_queue* cq, context* ctx, cl_command_type cmd)
  : m_context(ctx), m_command_queue(cq), m_command_type(cmd), m_wait_count(1)
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;
  debug::add_command_type(this,cmd);

  for (auto& cb : sg_constructor_callbacks)
    cb(this);

  XOCL_DEBUG(std::cout,"xocl::event::event(",m_uid,")\n");
}

event::
event(command_queue* cq, context* ctx, cl_command_type cmd, cl_uint num_deps, const cl_event* deps)
  : event(cq,ctx,cmd)
{
  for (auto dep : get_range(deps,deps+num_deps)) {
    XOCL_DEBUG(std::cout,"event(",m_uid,") depends on event(",xocl(dep)->get_uid(),")\n");
    xocl(dep)->chain(this);
  }
  debug::add_dependencies(this,num_deps,deps);
  profile::log_dependencies(this, num_deps, deps);
}

event::
~event()
{
  XOCL_DEBUG(std::cout,"xocl::event::~event(",m_uid,")\n");
  for (auto& cb : sg_destructor_callbacks)
    cb(this);
}

cl_int
event::
set_status(cl_int s)
{
  // Retain so that event is guaranteed to remain alive for the
  // duration of this function.  We could reorder to run callbacks
  // first, but its vital to signal condition variables first to keep
  // things rolling.  Only necessary to retain if CL_COMPLETE.
  // - ex1) user thread waits for CL_COMPLETE triggered by this
  //   call at which point it squeezes in clReleaseEvent before this
  //   function runs the callbacks.
  bool complete = (s==CL_COMPLETE);
  ptr<xocl::event> retain(complete?this:nullptr);

  {
    std::lock_guard<std::mutex> lk(m_mutex);

    // Some enqueue operations may need to record CL_RUNNING
    // without knowing that the enqueue operation is invoked
    // multiple times.  See api/enqueue.cpp migrate_buffer
    if (s==m_status) {
      assert(s==CL_RUNNING);
      return s;
    }

    XOCL_DEBUG(std::cout,"event(",m_uid,") [",to_string(m_status),"->",to_string(s),"]\n");

    std::swap(m_status,s);
    time_set(m_status);
  } // lk

  //Make the profile logging calls before notifying the event
  //and before removing it from queue. Otherwise the main could exit
  //deleting datastrucutres while the profile call is ongoing (CR-1003505)
  profile::log(this,m_status);

  if (complete) {
    // Run callbacks before notifying the event and before removing it from queue
    // If events are notified or removed from queue before running callbacks then
    // the user thread calling clWaitForEvents() or clFinish() will unblock and
    // proceed (or exit main() as in CR-1002026) with the assumption that callback finished.
    run_callbacks(CL_COMPLETE);

    m_event_complete.notify_all();

    // remove the completed event from queue (submitted queue)
    // before event_scheduler attempts to submit next event.
    queue_remove();   // 1 (order matters)
    for (auto& c : m_chain) // not a race, since m_chain is blocked by CL_COMPLETE
      c->submit();
  }

  return s;
}

bool
event::
queue(bool blocking_submit)
{
  bool queued = false;
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    queued = queue_queue();
    if (queued) {
      XOCL_DEBUG(std::cout,"event(",m_uid,") [",to_string(m_status),"->",to_string(CL_QUEUED),"]\n");
      m_status = CL_QUEUED;
      profile::log(this,m_status);
      time_set(CL_QUEUED);
    }
  }

  assert(queued);

  // Submit the event now if possible (event is created with wait_count=1)
  submit();

  if (blocking_submit) {
    // block current thread until event has truly submitted
    std::unique_lock<std::mutex> lk(m_mutex);
    while (m_status==CL_QUEUED)
      m_event_submitted.wait(lk);
  }

  return queued;
}

bool
event::
submit()
{
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (--m_wait_count) {
      XOCL_DEBUG(std::cout,"event(",m_uid,") cannot submit wait_count(",m_wait_count,")\n");
      return false;
    }

    XOCL_UNUSED auto submitted = queue_submit();
    assert(submitted);

    XOCL_DEBUG(std::cout,"event(",m_uid,") [",to_string(m_status),"->",to_string(CL_SUBMITTED),"]\n");
    m_status = CL_SUBMITTED;
    profile::log(this,m_status);
    time_set(CL_SUBMITTED);
  }

  m_event_submitted.notify_all();

  if (is_hard())
    trigger_enqueue_action();

  return true;
}

bool
event::
abort(cl_int status,bool fatal)
{
  if (status>=0)
    throw xocl::error(CL_INVALID_VALUE,"event::abort() called with non negative value");

  // This function feels overly complicated
  std::lock_guard<std::mutex> lk(m_mutex);

  // Collect all events in current context
  std::vector<event*> events;
  for (auto q : m_context->get_queue_range())
    range_copy(q->get_event_range(),std::back_inserter(events));

  // Abort the chain of events
  std::vector<event*> aborts(1,this);
  while (aborts.size()) {
    auto abort_ev = aborts.back();
    aborts.pop_back();
    XOCL_DEBUG(std::cout,"event(",m_uid,") [",to_string(m_status),"->",to_string(status),"]\n");

    // Only abort queued events unless fatal abort
    if (abort_ev==this && (fatal || abort_ev->m_status==CL_QUEUED)) {
      abort_ev->m_status = status;  // abort ev
      abort_ev->queue_abort(fatal); // remove from queue if any
      m_event_complete.notify_all();
    }
    else if (abort_ev!=this) {
      // recursively abort event that depends on this
      abort_ev->abort(status,fatal);
    }

    for (auto ev : events) {
      if (ev->waits_on(abort_ev))
        aborts.push_back(ev);
    }
  }

  return true;
}

void
event::
wait() const
{
  XOCL_DEBUG(std::cout,"xocl::event::wait(",m_uid,")\n");
  std::unique_lock<std::mutex> lk(m_mutex);
  while (m_status>0)  // (<0 => aborted) (==0 => CL_COMPLETE)
    m_event_complete.wait(lk);
}

void
event::
add_callback(callback_function_type fcn)
{
  bool complete = false;
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    if ((complete=(m_status==CL_COMPLETE))==false) {
      if (!m_callbacks)
        m_callbacks = xrt::make_unique<callback_list>();
      m_callbacks->emplace_back(std::move(fcn));
    }
  }

  // If event was already complete, then the callback was not installed
  // but should still be called
  if (complete)
    fcn(CL_COMPLETE);
}

void
event::
run_callbacks(cl_int status)
{
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_callbacks)
      return;
  }

  // cannot lock mutex while calling the callbacks
  // so copy address of callbacks while holding the lock
  // the execute callbacks without lock
  std::vector<callback_function_type*> copy;
  copy.reserve(m_callbacks->size());

  {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::transform(m_callbacks->begin(),m_callbacks->end()
                   ,std::back_inserter(copy)
                   ,[](callback_function_type& cb) { return &cb; });
  }

  for (auto cb : copy)
    (*cb)(status);
}

void
event::
register_constructor_callbacks(event_callback_type&& aCallback)
{
  sg_constructor_callbacks.emplace_back(std::move(aCallback));
}

void
event::
register_destructor_callbacks(event_callback_type&& aCallback)
{
  sg_destructor_callbacks.emplace_back(std::move(aCallback));
}


void
event::
chain(event* ev)
{
  // assert(ev is locked because it is being enqueued || called from "ev" event ctor);
  assert(ev->m_status == -1); // ev is being enq'ed or ctored

  std::lock_guard<std::mutex> lk(m_mutex);
  if (m_status == CL_COMPLETE)
    return;
  m_chain.push_back(ev);
  ++ev->m_wait_count;
}

bool
event::
chains(const event* ev) const
{
  std::lock_guard<std::mutex> lk(m_mutex);
  return std::find(m_chain.begin(),m_chain.end(),ev)!=m_chain.end();
}

bool
event::
waits_on(const event* ev) const
{
  return ev->chains(this);
}

bool
event::
queue_queue()
{
  // TODO: retain unconditionally regardless of type of event
  // no need for command queue to retain event if retained here
#if 0
  retain();
  return (is_soft() || m_command_queue->queue(this));
#endif

  if (is_soft()) {
    retain();
    return true;
  }

  return (m_command_queue->queue(this));
}

bool
event::
queue_submit()
{
  return (is_soft() || m_command_queue->submit(this));
}

bool
event::
queue_remove()
{
  // TODO: release unconditionally regardless of type of event
  // no need for command queue to release event if released here
#if 0
  release();
  return (is_soft() || m_command_queue->remove(this));
#endif

  if (is_soft()) {
    release();
    return true;
  }

  return (m_command_queue->remove(this));
}

bool
event::
queue_abort(bool fatal)
{
  // TODO: release unconditionally regardless of type of event
  // no need for command queue to release event if released here
#if 0
  release();
  return (is_soft() || m_command_queue->abort(this,fatal));
#endif

  if (is_soft()) {
    release();
    return true;
  }

  return (m_command_queue->abort(this,fatal));
}

ptr<event>
create_event(command_queue* cq, context* ctx, cl_command_type cmd, cl_uint num_deps, const cl_event* deps)
{
  using ew_event   = event;                           // event with waitlist
  using ep_event   = event_with_profiling<event>;     // event with profiling
  using epw_event  = event_with_profiling<ew_event>;  // event with profile and waitlist

  // Events with debugging
  using ed_event   = event_with_debugging<event>;     // debug event
  using edw_event  = event_with_debugging<ew_event>;  // debug event with waitlist
  using edp_event  = event_with_debugging<ep_event>;  // debug event with profiling
  using edpw_event = event_with_debugging<epw_event>; // debug event with profiling and waitlist

  assert(!cq || cq->get_context()==ctx);
  static bool app_debug = xrt::config::get_app_debug();

  ptr<event> retval;

  if (cq && cq->is_profiling_enabled()) {
    if (num_deps)
      retval = app_debug
        ? new edpw_event(cq,ctx,cmd,num_deps,deps)
        : new epw_event(cq,ctx,cmd,num_deps,deps);
    else
      retval = app_debug
        ? new edp_event(cq,ctx,cmd)
        : new ep_event(cq,ctx,cmd);
  }
  else {
    if (num_deps)
      retval = app_debug
        ? new edw_event(cq,ctx,cmd,num_deps,deps)
        : new ew_event(cq,ctx,cmd,num_deps,deps);
    else
      retval = app_debug
        ? new ed_event(cq,ctx,cmd)
        : new event(cq,ctx,cmd);
  }

  // Release the refcount returned by event ctor, it is now captured
  // a second time by the retval (ptr<event).
  retval->release();
  return retval;
}

ptr<event>
create_hard_event(cl_command_queue q,cl_command_type cmd, cl_uint num_deps, const cl_event* deps)
{
  return create_event(xocl::xocl(q),xocl::xocl(q)->get_context(),cmd,num_deps,deps);
}

ptr<event>
create_soft_event(cl_context ctx, cl_command_type cmd, cl_uint num_deps, const cl_event* deps)
{
  return create_event(nullptr,xocl::xocl(ctx),cmd,num_deps,deps);
}

} // xocl
