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

#ifndef xrt_util_event_h_
#define xrt_util_event_h_

#include "xrt/util/error.h"

namespace xrt { 

/**
 * Event class providing an abstraction over different event implementations.
 *
 * Class uses type erasure to encapsulate any type of events while
 * providing a consistent API (wait(), ready()) to clients.
 *
 * The enclosed concrete event types must define
 *   value_type: 
 *     type of the value encapsulated by the event
 *   value_type wait() const: 
 *     waits on the event and returns its value
 *   bool ready() const: 
 *     returns immediately with true if event is ready, false otherwise
 *
 * A sample event class that fits the above requirements is:
 *
 *   class myevent
 *   {
 *     typedef int value_type;
 *     value_type wait() const;
 *     bool ready() const;
 *   };
 *
 *   myevent ev = ...;
 *   xrt::event ev(std::move(myevent));
 *   int i = ev.get<int>();
 */
class event
{
  struct iholder
  {
    virtual ~iholder() {}
    virtual void wait() const = 0;
    virtual bool ready() const = 0;
  };

  template <typename ValueType, int dummy=0>
  struct value_holder : iholder
  {
    mutable ValueType m_value;
    mutable bool m_valid;
    value_holder() : m_valid(false) {}
    void setValue(ValueType&& v) const { m_value = std::move(v); m_valid = true; }
    bool isValid() const { return m_valid; }
    // The ValueType does not have to be copyable so can only be
    // moved.  Result is that get() can be called only once
    ValueType get() const { wait(); return std::move(m_value); }
  };

  template <int dummy>
  struct value_holder<void,dummy> : iholder
  {
    mutable bool m_valid;
    value_holder() : m_valid(false) {}
    void setValue(void) const { m_valid = true; }
    bool isValid() const { return m_valid; }
    void get() const { wait(); }
  };

  template <typename EventType, typename ValueType>
  struct event_holder : value_holder<ValueType>
  {
    typedef ValueType value_type;
    EventType m_held;
    event_holder(EventType&& e) : m_held(std::move(e)) {}
    void wait()  const { if (!this->isValid()) this->setValue(m_held.wait()); }
    bool ready() const { return this->isValid() ? true : m_held.ready(); }
  };

  // Argh, avoid specialization, find a better way to compose setValue
  // The needs for this specialiation points to poor design.
  template <typename EventType>
  struct event_holder<EventType,void> : value_holder<void>
  {
    typedef void value_type;
    EventType m_held;
    event_holder(EventType&& e) : m_held(std::move(e)) {}
    void wait()  const { if (!this->isValid()) {m_held.wait(); this->setValue();} }
    bool ready() const { return this->isValid() ? true : m_held.ready(); }
  };

  std::unique_ptr<iholder> m_content;

  template <typename ValueType>
  value_holder<ValueType>*
  value_cast() const noexcept
  {
    return dynamic_cast<value_holder<ValueType>*>(m_content.get());
  }
  

public:

  event() 
    : m_content(nullptr)
  {}

  event(event&& rhs)
    : m_content(std::move(rhs.m_content))
  {
    // Invalidate rhs to avoid double delete
    rhs.m_content = nullptr;
  }

  template <typename EventType>
  event(EventType&& e)
    : m_content(new event_holder<EventType,typename EventType::value_type>(std::forward<EventType>(e)))
  {}

  event&
  operator=(event&& e)
  {
    std::swap(m_content,e.m_content);
    return *this;
  }

  bool
  ready() const
  {
    return m_content 
      ? m_content->ready()
      : true;
  }

  void
  wait() const
  {
    if (m_content)
      m_content->wait();
  }

  template <typename ValueType>
  ValueType 
  get() const
  {
    value_holder<ValueType>* vt = value_cast<ValueType>();
    if (!vt)
      throw xrt::error("invalid event cast");
    return vt->get();
  }
};

/**
 * Simple event class for wrapping synchronous return values
 */
template <typename T>
class typed_event
{
  T m_value;
public:
  typedef T value_type;

  typed_event(T&& t) : m_value(std::move(t)) {}

  T wait() const { return m_value; }
  bool ready() const { return true; }
};

template <>
class typed_event<void>
{
public:
  typedef void value_type;
  void wait() const { }
  bool ready() const { return true; }
};

} // xrt

#endif


