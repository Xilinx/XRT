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

#include "command_queue.h"
#include "context.h"
#include "device.h"
#include "event.h"

#include "xocl/api/plugin/xdp/profile.h"

#include <algorithm>
#include <iostream>
#include <cassert>

#ifdef _WIN32
#pragma warning ( disable : 4267 )
#endif

namespace {

static xocl::command_queue::commandqueue_callback_list sg_constructor_callbacks;
static xocl::command_queue::commandqueue_callback_list sg_destructor_callbacks;

}

namespace xocl {

command_queue::
command_queue(context* ctx, device* device, cl_command_queue_properties props)
  : m_context(ctx), m_device(device), m_props(props)
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  if (xrt::config::get_profile())
    m_props |= CL_QUEUE_PROFILING_ENABLE;

  XOCL_DEBUG(std::cout,"xocl::command_queue::command_queue(",m_uid,")\n");
  //appdebug::add_command_queue(this);

  for (auto& cb : sg_constructor_callbacks)
    cb(this);

  ctx->add_queue(this);
}

command_queue::
~command_queue()
{
  wait();

  XOCL_DEBUG(std::cout,"xocl::command_queue::~command_queue(",m_uid,")\n");
  //appdebug::remove_command_queue(this);

  for (auto& cb : sg_destructor_callbacks)
    cb(this);

  assert(m_events.empty());
  m_context->remove_queue(this);
}

bool
command_queue::
queue(event* ev)
{
  bool ooo = m_props.test(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
  XOCL_DEBUG(std::cout,"queue(",m_uid,") queues event(",ev->get_uid(),")\n");

  std::lock_guard<std::mutex> lk(m_events_mutex);
  if (!ooo && m_last_queued_event.get()) {
    m_last_queued_event->chain(ev);

    auto tmp_lval = static_cast<cl_event>(m_last_queued_event.get());
    xocl::profile::log_dependencies(ev, 1, &tmp_lval);
  }

  if (ooo) {
    for (auto b: m_barriers)
      b->chain(ev);

    xocl::profile::log_dependencies(ev, m_barriers.size(), reinterpret_cast<cl_event*>(m_barriers.data()) );

    if (ev->get_command_type()==CL_COMMAND_BARRIER)
      m_barriers.push_back(ev);
  }

  m_events.insert(ev);
  m_last_queued_event = ev;
  ev->retain();

  return true;
}

bool
command_queue::
submit(event* ev)
{
  // This function is really not necessary, it doesn't do anything
  // but is here for symmetry and to allow sanity checks.

  // pre-condition: ev is locked

  // submit must never fail, event calls submit when its wait count
  // reaches 0, if it doesn't submit, it will stay queued forever.
  //
  // submit must *not* lock the queue
  //  1 - event(3)::queue()     // lock(3)
  //   2 - queue::queue(3)      // lock(queue)
  //   4  - event(2)->chain(3)  // want lock(2) // ok ...
  //  3 - event(2)::submit()    // lock(2)
  //   4 - queue::submit(2)     // want lock(queue) // bad ...
  // break by not locking in queue::submit

  //assert(m_events.find(ev)!=m_events.end());
  assert(ev->m_status==CL_QUEUED);
  //assert(m_props.test(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) || m_submitted==0);

  XOCL_DEBUG(std::cout,"queue(",m_uid,") submits event(",ev->get_uid(),")\n");
  return true;
}

bool
command_queue::
remove(event* ev)
{
  std::lock_guard<std::mutex> lk(m_events_mutex);

  auto it = m_events.find(ev);
  if (it==m_events.end())
    throw xocl::error(CL_INVALID_EVENT,"event " + ev->get_suid() + " never submitted");
  m_events.erase(it);
  if (m_last_queued_event==ev)
    m_last_queued_event = nullptr;

  if ((ev->get_command_type()==CL_COMMAND_BARRIER) && (m_props.test(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE)))  {
    auto bit = std::find(m_barriers.begin(),m_barriers.end(),ev);
    assert(bit!=m_barriers.end());
    m_barriers.erase(bit);
  }

  ev->release();
  if (m_events.empty())
    m_has_events.notify_all();

#if 0
  if ((m_events.size() % 1000)==0) {
    static unsigned long last = time_ns();
    auto now = time_ns();
    XOCL_PRINT(std::cout,m_events.size()," ",(now-last)*1e-6,"\n");
    last = now;
  }
#endif

  return true;
}

bool
command_queue::
abort(event* ev,bool)
{
  return remove(ev);
}

void
command_queue::
wait() const
{
  XOCL_DEBUG(std::cout,"xocl::command_queue::wait(",m_uid,")\n");
  std::unique_lock<std::mutex> lk(m_events_mutex);
  while (m_events.size())
    m_has_events.wait(lk);
}

void
command_queue::
flush() const
{
  XOCL_DEBUG(std::cout,"xocl::command_queue::flush(",m_uid,")\n");
  std::unique_lock<std::mutex> lk(m_events_mutex);
  while (m_events.size())
    m_has_events.wait(lk);
}

command_queue::queue_lock
command_queue::
wait_and_lock() const
{
  XOCL_DEBUG(std::cout,"xocl::command_queue::wait_and_lock(",m_uid,")\n");
  std::unique_lock<std::mutex> lk(m_events_mutex);
  while (m_events.size())
    m_has_events.wait(lk);
  return queue_lock(std::move(lk));
}
void
command_queue::
register_constructor_callbacks(commandqueue_callback_type&& aCallback)
{
  sg_constructor_callbacks.emplace_back(std::move(aCallback));
}

void
command_queue::
register_destructor_callbacks(commandqueue_callback_type&& aCallback)
{
  sg_destructor_callbacks.emplace_back(std::move(aCallback));
}
}
