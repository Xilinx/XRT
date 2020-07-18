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

#ifndef _XRT_PIPELINE_H_
#define _XRT_PIPELINE_H_

// This file is work-in-progress and subject to removal at any time

#include "experimental/xrt_enqueue.h"

#ifdef __cplusplus
# include <memory>
# include <vector>
# include <tuple>
#endif

#ifdef __cplusplus

namespace xrt {

// This is WIP

// - pipeline must be a synchronous task in order for composition to
//   work
// - work out how to parameterize stage function without binding
//   a stage to specific arguments

/**
 * class xrt::pipeline -- connected group of tasks
 *
 * A pipeline executes a set of functions in a specified order.
 * Once stages are added to the pipeline, a control flow 
 * between the stages is defined.  The flow defines how the 
 * pipeline stages execute essentially forming a DAG of
 * stages where all parent stages must finish executing before
 * a child stage can be started.
 *
 * The execution is managed by xrt::event objects.  Each stage
 * function returns an event that is used when enqueue child
 * stages.
 * 
 * A pipeline itself can be stage of another pipeline.
 */
class pipeline_impl;
class pipeline
{
  friend class pipeline_impl;

  // class stage - holds a stage function, which can be enqueued in an
  // xrt::event_queue
  class stage
  {
    struct stage_holder
    {
      virtual xrt::event
      enqueue(xrt::event_queue& q, const std::vector<xrt::event>& deps) = 0;
    };

    template <typename Callable>
    struct stage_type : stage_holder
    {
      Callable m_held;
      stage_type(Callable&& c)
        : m_held(std::move(c))
      {}

      xrt::event
      enqueue(xrt::event_queue& q,const std::vector<xrt::event>& deps)
      {
        return q.enqueue_with_waitlist(m_held, deps);
      }
    };

    std::unique_ptr<stage_holder> m_content;

  public:
    stage() : m_content(nullptr)
    {}

    stage(stage&& rhs) : m_content(std::move(rhs.m_content))
    {}

    template <typename Callable>
    stage(Callable&& c)
      : m_content(new stage_type<Callable>(std::forward<Callable>(c)))
    {}

    xrt::event
    enqueue(xrt::event_queue& q,const std::vector<xrt::event>& deps)
    {
      return m_content->enqueue(q, deps);
    }
  };


public:
  /**
   * Constructor - 
   *
   * @q:  Event queue on which stage functions are enqueued
   *
   * The event queue (see @xrt_enqueue.h) is associated with one or
   * more event handlers that execute the enqueued functions. With the
   * knowledge of the stage functions properties, it is important to
   * ensure that the event queue has sufficient event handlers.  For
   * example, two syncronous stages can maybe execute concurrently,
   * but only if the event queue has at least two handlers.
   */
  pipeline(const xrt::event_queue& q);

  /**
   * execute() - Run the pipeline once
   *
   * @event:  Event that controls the start of the first stage
   */
  xrt::event
  execute(xrt::event event);

  /**
   * execute() - Run the pipeline once
   *
   * First stage of the pipeline can be started immediately
   */
  xrt::event
  execute()
  {
    xrt::event event;
    return execute(event);
  }

  /**
   * operator() - The pipeline is callable
   *
   * The pipeline itself can be used as a stage in another
   * pipeline.
   */
  xrt::event
  operator() (const xrt::event& event)
  {
    return execute(event);
  }

  /**
   * operator() - The pipeline is callable
   *
   * The pipeline itself can be used as a stage in another
   * pipeline.
   */
  xrt::event
  operator() ()
  {
    return execute();
  }

  /**
   * define the control flow graph -- todo
   */
  void
  set_flow_control()
  {
  }

  /**
   * emplace() - Add a callable to the pipeline
   */
  template <typename Callable>
  auto emplace(Callable&& c)
  {
    stage s(std::forward<Callable>(c));
    return std::make_tuple(std::ref(add_stage(std::move(s))));
  }

  /**
   * emplace() - Add callable functios to the pipeline
   */
  template <typename Callable, typename ...Callables>
  auto emplace(Callable&& c, Callables&&... cs)
  {
    // Argument order evaluation is not guaranteed, make sure that
    // stages are added to the vector in right order
    auto t = emplace(std::forward<Callable>(c));
    return std::tuple_cat(std::move(t), emplace(std::forward<Callables>(cs)...));
  }

private:
  const stage&
  add_stage(stage&& s);

private:
  std::shared_ptr<pipeline_impl> m_impl;
};

} // namespace xrt

#else
# error xrt_pipeline is only implemented for C++
#endif // __cplusplus

#endif
