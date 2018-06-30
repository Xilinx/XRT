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

#ifndef xocl_core_command_queue_h_
#define xocl_core_command_queue_h_

#include "xocl/core/object.h"
#include "xocl/core/refcount.h"
#include "xocl/core/property.h"

#include <vector>
#include <set>
#include <unordered_set>
#include <mutex>
#include <condition_variable>

namespace xocl {

class command_queue : public refcount, public _cl_command_queue
{
  using property_type = property_object<cl_command_queue_properties>;

  // while the queue shares ownership of an event, it does not
  // store queued and submitted events as references.  instead
  // it retains the event upon queuing and releases it when the
  // event is removed.
public:
  using event_queue_type = std::unordered_set<event*>;
  using event_iterator_type = event_queue_type::iterator;

  using commandqueue_callback_type = std::function<void(command_queue*)>;
  using commandqueue_callback_list = std::vector<commandqueue_callback_type>;

private:
  // Used to aquire a lock on this queue to prevent de/queing of event
  struct queue_lock 
  {
    std::unique_lock<std::mutex> m_lk;
    queue_lock(std::unique_lock<std::mutex>&& lk)
      : m_lk(std::move(lk))
    {}
  };

public:
  command_queue(context* ctx, device* device, cl_command_queue_properties props);
  virtual ~command_queue();

  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  context*
  get_context() const
  {
    return m_context.get();
  }

  device*
  get_device() const
  {
    return m_device.get();
  }

  const property_type&
  get_properties() const
  {
    return m_props;
  }

  property_type&
  get_properties()
  {
    return m_props;
  }

  /**
   * Check if profiling of commands in the command-queue is enabled.
   * 
   * @return
   *   true if profiling is enabled for this queue, false otherwise
   */
  bool
  is_profiling_enabled() const
  {
    return m_props.test(CL_QUEUE_PROFILING_ENABLE);
  }

  /**
   * Get range with events that are queued or submitted
   */
  range_lock<event_iterator_type>
  get_event_range()
  {
    std::unique_lock<std::mutex> lock(m_events_mutex);
    return range_lock<event_iterator_type>(m_events.begin(),m_events.end(),std::move(lock));
  }

  /**
   * Add event to the command queue
   *
   * @return
   *   true if successfully queued, false otherwise
   */
  bool
  queue(event*);

  /**
   * Submit event for execution
   *
   * @return
   *   true if successfully submitted, false otherwise
   */
  bool
  submit(event*);

  /**
   * Remove event from queue.
   *
   * @return
   *   true if successully removed from queue, false otherwise
   */
  bool
  remove(event* ev);

  /**
   * Abort event
   *
   * Unconditionaly remove an event from the command_queue.
   *
   * @param ev
   *   Event that is aborting
   * @param fatal (default: false)
   *   A fatal error has occurred.  If set, then submitted events
   *   are aborted also.
   * @return
   *   true if successully removed from queue, false otherwise
   */
  bool
  abort(event* ev,bool fatal=false);

  /**
   * Wait for all events to complete
   */
  void
  wait() const;

  /**
   * Wait for all enqueued events to be submitted
   */
  void
  flush() const;

  /**
   * Wait for all events to complete, then return a lock
   * that prevents new events from being enqueued.
   *
   * @returns
   *   A lock on the queue
   */
  queue_lock
  wait_and_lock() const;


  static void
  register_constructor_callbacks(commandqueue_callback_type&& aCallback);

  static void
  register_destructor_callbacks(commandqueue_callback_type&& aCallback);

private:
  unsigned int m_uid = 0;
  ptr<context> m_context;
  ptr<device> m_device;

  mutable std::mutex m_events_mutex;
  mutable std::condition_variable m_has_events;
  event_queue_type m_events;
  std::vector<event*> m_barriers;
  ptr<event> m_last_queued_event;
  property_type m_props;

  static commandqueue_callback_list m_constructor_callbacks;
  static commandqueue_callback_list m_destructor_callbacks;
};

} // xocl

#endif


