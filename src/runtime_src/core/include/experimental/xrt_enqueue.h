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

#ifndef _XRT_ENQUEUE_H_
#define _XRT_ENQUEUE_H_

#ifdef __cplusplus
# include <memory>
# include <vector>
# include <functional>
# include <algorithm>
# include <future>
#endif

#ifdef __cplusplus

namespace xrt {

// Default traits for callable objects used in specialization
// Overwritten for asynchronous objects, e.g. xrt::run
template <typename Callable>
struct callable_traits
{
  enum { is_async = false };
};

/**
 * class event_queue -- producer / consumer queue for tasks
 *
 * Used for asynchronous execution of synchronous operations.
 *
 * An event queue consumer is a 'event_handler', which can run on a
 * separate thread.  A event_queue can have any number of consumers.
 *
 * event_queue
 */
class event_queue_impl;
class event_impl;
class event_queue
{
  friend class event_queue_impl;
  friend class event_impl;
  // class task - wraps a typed callable operation
  class task
  {
    using event_ptr = std::shared_ptr<event_impl>;

    struct task_iholder
    {
      virtual ~task_iholder() {};
      virtual void execute(const event_ptr&) = 0;
    };

    // Wrap synchronous operation
    template <typename Callable>
    struct task_holder : public task_iholder
    {
      Callable m_held;

      task_holder(Callable&& t)
        : m_held(std::move(t))
      {}

      void execute(const event_ptr& evp)
      {
        m_held();                  // synchronous function
        event::notify(evp.get());  // notify event of completion
      }
    };

    // Wrap asynchronous operation
    template <typename Callable, typename ResultType>
    struct task_async_holder : public task_iholder
    {
      using future_type = std::shared_future<ResultType>;

      Callable m_held;
      std::shared_future<ResultType> m_future;

      task_async_holder(Callable&& t, const std::shared_future<ResultType>& f)
        : m_held(std::move(t)), m_future(f)
      {}

      void
      execute(const event_ptr& evp)
      {
        m_held();                 // start async operation
        auto& w = m_future.get(); // get waitable return value
        w.set_event(evp);         // waitable must notify event
      }
    };

    std::unique_ptr<task_iholder> m_content;

  public:
    task() : m_content(nullptr)
    {}

    task(task&& rhs) : m_content(std::move(rhs.m_content))
    {}

    // task() - task constructor for synchronous operation
    //
    // @c : callable object, a std::packaged_task
    template <typename Callable>
    task(Callable&& c)
      : m_content(new task_holder<Callable>(std::forward<Callable>(c)))
    {}

    // task() - task constructor for asynchronous operation
    //
    // @c : callable object, a std::packaged_task
    // @f : future for return value of callable
    template <typename Callable, typename ResultType>
    task(Callable&& c, const std::shared_future<ResultType>& f)
      : m_content(new task_async_holder<Callable, ResultType>(std::forward<Callable>(c), f))
    {}

    task&
    operator=(task&& rhs)
    {
      m_content = std::move(rhs.m_content);
      return *this;
    }

    operator bool() const
    {
      return m_content!=nullptr;
    }
    
    void
    execute(const event_ptr& evp)
    {
      m_content->execute(evp);
    }
  };  // class event_queue::task

public:
  // class event_queue::event - event based task execution
  class event
  {
  public:
    event(task&& t, const std::vector<event>& deps);

    static void
    notify(event_impl*);

  public:
    const std::shared_ptr<event_impl>&
    get_impl() const
    {
      return m_impl;
    }

  private:
    std::shared_ptr<event_impl> m_impl;
  };  // class event_queue::event

private:
  /** 
   * class event_queue::event_type - event for synchronous operations
   *
   * Wraps a std::future which can be waited on of value retrieved
   *
   * This event type is returned through event_queue::enqueue and can
   * be used to chain execution further events.
   */
  template <typename ResultType>
  class event_type : public event
  {
    using value_type = ResultType;
    using task_type = std::packaged_task<value_type()>;
    using future_type = std::future<value_type>;
    std::future<value_type> m_future;

  public:
    event_type(task_type&& t, future_type&& f, const std::vector<event>& deps)
      : event(std::move(t), deps)
      , m_future(std::move(f)) 
    {}

    /**
     * get() - Get the return value of the enqueued synchronous operation
     *
     * Return:  The return value after successful event / task execution
     */
    value_type
    get()
    {
      return m_future.get();
    }

    /**
     * wait() - Wait for the enqueued synchronous operation to complete
     *
     * Return:  The return value after successful event / task execution
     */
    void
    wait() const
    {
      m_future.wait();
    }
  }; // class event_queue::event_type


  /** 
   * class event_queue::event_async_type - event for asynchronous operations
   *
   * Wraps a std::shared_future which can be waited on.
   *
   * This event type is returned through event_queue::enqueue and can
   * be used to chain execution further events.
   */
  template <typename ResultType>
  class event_async_type : public event
  {
    using value_type = ResultType;
    using task_type = std::packaged_task<value_type()>;
    std::shared_future<value_type> m_future;

  public:
    event_async_type(task_type&& t, const std::shared_future<value_type>& f, const std::vector<event>& deps)
      : event(task(std::move(t), f), deps)
      , m_future(f) 
    {}
    
    /**
     * wait() - Wait for the async operation to complete
     *
     * Waiting on an async event is waiting on the waitable object
     * returned after launching the async operation. The return 
     * value of an asynchronous operation is not available and nothing
     * is returned to host application
     */
    void
    wait() const
    {
      auto& waitable = m_future.get();
      waitable.wait();
    }
  };

private:
  void
  add_event(const event& ev);

  template <typename ResultType, bool async = false>
  struct enqueuer
  {
    using value_type = ResultType;
    using task_type = std::packaged_task<value_type()>;

    static auto
    add(task_type&& t, const std::vector<event>& deps)
    {
      std::future<value_type> f(t.get_future());
      return event_type<value_type>(std::move(t), std::move(f), deps);
    }
  };
    
  template <typename ResultType>
  struct enqueuer<ResultType, true>
  {
    using value_type = ResultType;
    using task_type = std::packaged_task<value_type()>;

    static auto
    add(task_type&& t, const std::vector<event>& deps)
    {
      std::shared_future<value_type> f(t.get_future());
      return event_async_type<value_type>(std::move(t), f, deps);
    }
  };

public:
  event_queue();

  /**
   * enqueue_with_waitlist() - Enqueue a callable with dependencies
   *
   * @c     : Callable function
   * @deps  : Event dependencies to complete before executing @c
   * @args  : Arguments to callable functions
   * Return : Event that can be waited on or chained with
   */
  template <typename Callable, typename ...Args>
  auto
  enqueue_with_waitlist(Callable&& r, const std::vector<event>& deps, Args&&... args)
  {
    using value_type = decltype(r(std::forward<Args>(args)...));
    using task_type = std::packaged_task<value_type()>;
    using task_traits = callable_traits<value_type>;
    task_type t(std::bind(std::forward<Callable>(r), std::forward<Args>(args)...));
    auto event = enqueuer<value_type,task_traits::is_async>::add(std::move(t), deps);
    add_event(event);
    return event;
  }
  
  /**
   * enqueue() - Enqueue a callable
   *
   * @c     : Callable function
   * @args  : Arguments to callable functions
   * Return : Event that can be waited on or chained with
   */
  template <typename Callable, typename ...Args>
  auto
  enqueue(Callable&& r, Args&&... args)
  {
    return enqueue_with_waitlist(r, {}, std::forward<Args>(args)...);
  }

public:
  event_queue_impl*
  get_impl() const
  {
    return m_impl.get();
  }
private:
  std::shared_ptr<event_queue_impl> m_impl;
};

/**
 * class task_handler_async - Asyncrhonous task handler
 *
 * A task handler is a consumer of a task queue.  The 
 * asynchronous task handler executes tasks on a separate 
 * thread. 
 *
 * The task_handler shares ownership of a task queue so
 * that the queue stays alive as long as the handler exists.
 * Upon handler deletion, the queue is notified to ensure that
 * the task handler thread is stopped properly.
 */
class event_handler_impl;
class event_handler
{
public:
  /**
   * event_handler() - Construct and assign event_queue
   *
   * @q : Event queue producing work for this handler
   */
  event_handler(const event_queue& q);
private:
  std::shared_ptr<event_handler_impl> m_impl;
};

/**
 * xrt::event - Alias for event_queue::event
 */
using event = event_queue::event;

} // namespace xrt

#else
# error xrt_enqueue is only implemented for C++
#endif // __cplusplus


#endif
