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

#ifndef xocl_core_event_h_
#define xocl_core_event_h_

#include "xocl/core/object.h"
#include "xocl/core/range.h"
#include "xocl/core/refcount.h"
#include "xocl/core/time.h"
#include "xocl/core/debug.h"
#include "xocl/core/error.h"
#include "xocl/core/execution_context.h"

#include "xrt/config.h"

#include <vector>
#include <functional>
#include <iostream>

namespace xocl {

/**
 * The event class consists of a base event class and a
 * derived event_with_wait_list class.
 *
 * An event can be associated with a task to execute via
 * the schedule member function, which queues the event for
 * execution.  An event is triggered by an event scheduler
 * after all its dependencies have been resolved.
 */
class event : public refcount, public _cl_event
{
  using callback_function_type = std::function<void(cl_int)>;
  using callback_list = std::vector<callback_function_type>;

  friend class command_queue;

public:
  using event_vector_type = std::vector<ptr<event>>;
  using event_iterator_type = ptr_iterator<event_vector_type::iterator>;

  using event_callback_type = std::function<void(event*)>;
  using event_callback_list = std::vector<event_callback_type>;

  using action_enqueue_type = std::function<void (event*)>;
  using action_profile_type = std::function<void (event*, cl_int, const std::string&)>;
  using action_debug_type = std::function<void (event*)>;

  event(command_queue* cq, context* ctx, cl_command_type cmd);
  event(command_queue* cq, context* ctx, cl_command_type cmd, cl_uint num_deps, const cl_event* deps);
  virtual ~event();

  /**
   */
  unsigned int
  get_uid() const 
  {
    return m_uid;
  }

  /**
   */
  std::string
  get_suid() const 
  {
    return std::to_string(m_uid);
  }

  /**
   */
  context*
  get_context() const 
  {
    return m_context.get();
  }

  /**
   */
  command_queue*
  get_command_queue() const 
  {
    return m_command_queue.get();
  }

  /**
   */
  void
  set_enqueue_action(action_enqueue_type&& action)
  {
    m_enqueue_action = std::move(action);
  }

  /**
   * Trigger the enqueue action if any.  
   *
   * This function is primarily used by the event scheduler as part of
   * launcing an event.  If an event doesn't have an enqueue action,
   * the event by default is complete, otherwise the enqueue action
   * controls marking the event complete.   The latter is because some
   * event actions are run without making the event complete.
   */
  void
  trigger_enqueue_action()
  {
    if (m_enqueue_action)
      m_enqueue_action(this);
    else
      set_status(CL_COMPLETE);
  }

  /**
   * Set the action to run when logging profile data
   *
   * This function is supposed to set the action in the
   * event_with_profiling class, but currently runtime
   * collects profile data event when the command queue
   * doesn't enable profiling.  
   */
  /*virtual*/ void
  set_profile_action(event::action_profile_type&& action) 
  {
    if (xrt::config::get_profile())
      m_profile_action = std::move(action);
  }

  /**
   * Trigger the profiling action
   *
   * This function is supposed to set the action in the
   * event_with_profiling class, but currently runtime
   * collects profile data event when the command queue
   * doesn't enable profiling.  
   */
  /*virtual*/ void
  trigger_profile_action(cl_int status, const std::string& cuname= "") 
  {
    if (m_profile_action)
      m_profile_action(this,status,cuname);
  }
  /**
   * This is valid only with event_with_debugging
   */
  virtual void
  set_debug_action(event::action_debug_type&& action)
  {
    throw std::runtime_error("Internal error. Application debug not enabled, unable to set debug action");
  }

  /**
   * This is valid only with event_with_debugging
   */
  virtual void
  trigger_debug_action()
  {
    throw xocl::error(DBG_EXCEPT_DBG_DISABLED, "Application debug not enabled");
  }

  /**
   * Returns a range_lock object of chained events, if the lock is acquired
   * This is meant to be used only for application debug.
   */
  range_lock<event_iterator_type>
  try_get_chain() 
  {
    std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);
    if (!lk.try_lock())
      throw xocl::error(DBG_EXCEPT_LOCK_FAILED, "Failed to secure lock on event");
    return range_lock<event_iterator_type>(m_chain.begin(),m_chain.end(),std::move(lk));
  }

  // for the time being the status is changed all over the place
  // in the old rt code.   future should bring status entirely within 
  // this class with no external setter.
  cl_int
  set_status(cl_int s);

  // likely temporary
  cl_int
  get_status() const
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_status;
  }

  /*
   * Try-locks to read m_status, returns -1 if cannot obtain lock
   * This is currently called from the functions that are invoked  from the debugger.
   */
  cl_int
  try_get_status() const
  {
     std::unique_lock<std::mutex> lk(m_mutex, std::defer_lock);
     if (!lk.try_lock())
       throw xocl::error(DBG_EXCEPT_LOCK_FAILED, "Failed to secure lock on event object");
     return m_status;
  }

  /**
   * Get the command type associated with this event
   *
   * @return
   *   Command type
   */
  cl_command_type
  get_command_type() const
  {
    return m_command_type;
  }

  /**
   * Set the command type on this event.
   *
   * Pre-condition (unchecked): Event is not yet scheduled 
   * No need to lock if pre-condition is met.
   *
   * @param ct
   *   Command type
   */
  void
  set_command_type(cl_command_type ct)
  {
    m_command_type = ct;
  }

  /**
   * Hook for overriding the autmatic time setting of 
   * a profiling event.
   *
   * Normally profiling time is set through change of
   * event status. This function can be used to explicitly
   * set the profiling time associated with argument state.
   *
   * @param status
   *    Associated specified time with status
   * @param ns
   *   Override time with argument nano seconds
   */
  void
  set_profiling_time(cl_int status, cl_ulong ns)
  {
    std::lock_guard<std::mutex> lk(m_mutex);
    time_set(status,ns);
  }

  /**
   * Queue the event on the command queue
   *
   * Once an event is queued it will submit if and only if all event
   * dependencies are statisfied.. Upon return, the event status is
   * either CL_QUEUED or CL_SUBMITTED
   *
   * @param blocking_submit
   *   if true, then the function will block until it can is
   *   submitted, that is, it will block until all event dependencies
   *   are satisfied.
   * @return
   *   true if event was queued successfully, false otherwise.
   */
  bool 
  queue(bool blocking_submit=false);

  /**
   * Abort (terminate) this event and chain of events that wait 
   * on this event.
   *
   * This is currently an somewhat expensive operation as event
   * wait chain has to be computed.  All events waiting either 
   * directly or indirectly (this event in transitive fanout of
   * a waitlist) on this event are aborted.
   *
   * @param status
   *   Set the status of aborted events to this value.  Must be
   *   a negative value.
   * @param fatal (default: false)
   *   A fatal error has occurred.  If set, then submitted events
   *   are aborted also.
   * @return
   *   true if aborted successfully, false on error
   */
  bool
  abort(cl_int status,bool fatal=false);

  /**
   * Wait for this event to complete
   */
  void
  wait() const;

  /**
   * If a profiling event, then support return requested values
   *
   * @return
   *   Time of status change, or 0 if profiling is not enabled
   */
  virtual cl_ulong
  time_queued() const { return 0; }
  virtual cl_ulong
  time_submit() const { return 0; }
  virtual cl_ulong
  time_start()  const { return 0; }
  virtual cl_ulong
  time_end()    const { return 0; }

  /**
   * Check if this event is a software event
   *
   * @return
   *   true if hardware event, false otherwise
   */
  bool
  is_hard() const { return m_command_queue.get()!=nullptr; }

  /**
   * Check if this event is a hardware event
   *
   * Software events are not associated with a command queue, hence
   * are not processed by event_scheduler
   *
   * @return
   *   true if not a hardware event, false otherwise
   */
  bool
  is_soft() const { return !is_hard(); }

  /**
   * Store an event call back
   *
   * @param fcn
   *  The callback function per OpenCL spec.  The function is
   #  already bound to user data and callback type
   */
  void
  add_callback(callback_function_type fcn);

  /**
   * Run all registered callbacks for this event
   *
   * @param status
   *   Run the callback with this status.  Disregard
   *   the events actual status.
   */
  void
  run_callbacks(cl_int status);

  /**
   * Set the context for kernel execution.
   */
  void
  set_execution_context(std::unique_ptr<execution_context>&& ec)
  {
    m_execution_context = std::move(ec);
  }

  /**
   * @return
   *   The execution context asssociated with this event, or nullptr
   *   if no context exists, e.g. the event is not an NDRange event
   */
  execution_context*
  get_execution_context() const
  {
    return m_execution_context.get();
  }

  static void register_constructor_callbacks(event_callback_type&& aCallback);
  static void register_destructor_callbacks(event_callback_type&& aCallback);


protected:
  /**
   * Add argument event to event chain
   *
   * It is guaranteed argument event is already locked (called from
   * queue::queue(ev)), or that this function is called from ev's 
   * contructor in which case there is no need to lock
   */
  void
  chain(event* ev);

private:
  /**
   * Submit this event for execution if possible
   *
   * @return
   *   true if submitted, false otherwise.
   */
  bool
  submit();

  /**
   * Check if this event chains argument event
   */
  bool
  chains(const event* ev) const;

  /**
   * Check if this event depends on argument event
   *
   * @param ev
   *   Event dependency to check for
   * @return
   *   true if argument event's chain contains this
   */
  bool
  waits_on(const event* ev) const;

  /**
   * If a profiling event, then record time at status change
   *
   * @param status
   *    Associated specified time with status
   */
  virtual void
  time_set(cl_int status) 
  {
    debug::time_log(this,status);
  }

  /**
   * If a profiling event, then record time at status change
   * 
   * @param status
   *    Associated specified time with status
   * @param ns
   *   Override time with argument nano seconds
   */
  virtual void
  time_set(cl_int status, cl_ulong ns) 
  {
    debug::time_log(this,status,ns);
  }

  /**
   * Queue this event on command queue
   *
   * @return
   *   true of successfully queued, false otherwise
   */
  bool
  queue_queue();

  /**
   * Submit this event on command queue
   *
   * @return
   *   true of successfully submitted, false otherwise
   */
  bool
  queue_submit();

  /**
   * Remove this event from command queue
   *
   * @return
   *   true of successfully removed, false otherwise
   */
  bool
  queue_remove();

  /**
   * Abort this event from command queue
   *
   * @param fatal
   *   If set then a fatal error has occurred.
   * @return
   *   true of successfully aborted, false otherwise
   */
  bool
  queue_abort(bool fatal=false);

private:
  unsigned int m_uid = 0;
  ptr<context> m_context;
  ptr<command_queue> m_command_queue;

  action_enqueue_type m_enqueue_action;

  // move to event_with_profiling when logging of 
  // profile data is controlled by command queue
  action_profile_type m_profile_action; 

  // execution context, probably should create some derived class
  std::unique_ptr<execution_context> m_execution_context;

  cl_int m_status = -1;
  cl_command_type m_command_type = 0;
  mutable std::mutex m_mutex;
  mutable std::condition_variable m_event_complete;
  mutable std::condition_variable m_event_submitted;

  // List of callback functions. On heap to avoid
  // allocation unless needed.
  std::unique_ptr<callback_list> m_callbacks;

  // List of chained events (events to submit upon completion)
  event_vector_type m_chain;

  // Number of events this event is waiting on.  This includes
  // explicit event depedencies and events that chain this
  unsigned int m_wait_count = 0;
};

/**
 * Event with profiling enabled
 *
 * This is s templated class on EventType such that other classes
 * can be inserted in between this class and the event base class, e.g.
 *    event_with_profiling -> event_with_wait_list -> event
 *    event_with_profiling -> my_class -> event
 */
template <typename EventType>
class event_with_profiling : public EventType
{
public:
  template <typename ...Args>
  event_with_profiling(Args&&... args)
    : EventType(std::forward<Args>(args)...)
  {}

  virtual ~event_with_profiling()
  {}

  void
  time_set(cl_int status)
  {
    auto ns = xocl::time_ns();
    time_set(status,ns);
    debug::time_log(this,status,ns);
  }

  void
  time_set(cl_int status, cl_ulong ns)
  {
    if (status==CL_QUEUED)
      m_queued = ns;
    else if (status==CL_SUBMITTED)
      m_submit = ns;
    else if (status==CL_RUNNING)
      m_start = ns;
    else if (status==CL_COMPLETE)
      m_end = ns;
    
    // CANNOT CALL OUTSIDE of xocl with this while this is locked
    // log_profile_data(this);  
  }

#if 0 // see comment in base class event
  /**
   * Set the action to run when logging profile data
   */
  void
  set_profile_action(event::action_profile_type&& action)
  {
    m_profile_action = std::move(action);
  }

  void
  trigger_profile_action(cl_int status, const std::string& cuname="")
  {
    if (m_profile_action)
      m_profile_action(this,status,cuname);
  }
#endif

  cl_ulong
  time_queued() const { return m_queued; }
  cl_ulong
  time_submit() const { return m_submit; }
  cl_ulong
  time_start()  const { return m_start ? m_start : m_end; }
  cl_ulong
  time_end()    const { return m_end; }

private:
  cl_ulong m_queued = 0;
  cl_ulong m_submit = 0;
  cl_ulong m_start  = 0;
  cl_ulong m_end    = 0;

  //event::action_profile_type m_profile_action;
};

/**
 * Event with debugging enabled
 *
 * This is s templated class on EventType such that other classes
 * can be inserted in between this class and the event base class, e.g.
 *    event_with_debugging -> event_with_profiling -> event_with_wait_list -> event
 *    event_with_debugging -> event_with_wait_list -> event
 *    event_with_debugging -> event
 *    event_with_debugging -> event_with_profiling -> event
 */
template <typename EventType>
class event_with_debugging : public EventType
{
public:
  template <typename ...Args>
  event_with_debugging(Args&&... args)
    : EventType(std::forward<Args>(args)...)
  {
    //appdebug::add_event(this);
  }

  virtual ~event_with_debugging()
  {
    //appdebug::remove_event(this);
  }

  /**
   * Set the action that will be called from the debugger
   * to get information about the event.
   */
  virtual void
  set_debug_action(event::action_debug_type&& action)
  {
      m_debug_action = std::move(action);
  }

  /**
   * The debug action set in the event will be called here.
   * The call is initiated from the debugger
   */
  virtual void
  trigger_debug_action()
  {
    if (m_debug_action)
      m_debug_action(this);
    else
      throw xocl::error(DBG_EXCEPT_NO_DBG_ACTION, "No debug action set in event");
  }

private:
  //This is called during app-debug to get information about the event
  event::action_debug_type m_debug_action;
};

/**
 * Create an event
 *
 * @param cq
 *   The command queue to associate with the event
 * @param ctx
 *   The context
 * @param num_deps (default: 0)
 *   The number of events passed in the deps argument
 * @param deps (default: nullptr)
 *   Array of events that must be complete before this event can
 *   trigger.
 * @return
 *   A sharing pointer that retains one reference to the 
 *   constructed event
 */
ptr<event>
create_event(command_queue* cq, context* ctx, cl_command_type cmd, cl_uint num_deps, const cl_event* deps);

/**
 * Create a hard event.
 *
 * A hard event must be enqueued only.  It will then be scheduled for
 * transition from state to state until complete.
 */
ptr<event>
create_hard_event(cl_command_queue cq, cl_command_type cmd, cl_uint num_deps=0, const cl_event* deps=nullptr);

/**
 * Create a soft event, which must be manually transitioned from state to state
 * 
 * A soft event must be manually enqueued,submitted, and marked complete.
 */
ptr<event>
create_soft_event(cl_context ctx, cl_command_type cmd, cl_uint num_deps=0, const cl_event* deps=nullptr);

/**
 * Helper function to allow scheduling event trigger
 * action on device work queue
 */
inline void
call_enqueue_action(event* ev)
{
  ev->trigger_enqueue_action();

  // Should probably be in the enqueue function itself???
  ev->set_status(CL_COMPLETE);
}

} // xocl

#endif


