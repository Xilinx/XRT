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

#ifndef xocl_core_refcount_h_
#define xocl_core_refcount_h_

#include <atomic>
#include <memory>
#include <iterator>
#include <type_traits>
#include <cassert>

namespace xocl {

/**
 * Base class for reference counted xocl objects.
 * 
 * Too bad that OpenCL API pointer objects cannot be redefined as
 * some smart pointer type. E.g. not much to do about
 * cl.h: typedef _cl_program* cl_program;
 *
 * This class wraps a reference counter.  It is still up to the the
 * implementation to use the count appropriately via the
 * implementation of CL API methods clRetain... and clRelease...
 */
class refcount 
{
  std::atomic<unsigned int> m_refcount;
 public:
  refcount() : m_refcount(1) {}
  
  /**
   * Increment refcount.
   */
  void
  retain() 
  { 
    assert(m_refcount>0);  // retaining a floating object
    ++m_refcount; 
  }

  /**
   * Decrement refcount.
   *
   * @return 
   *   true of refcount reaches zero, false otherwise
   */
  bool
  release() 
  { 
    assert(m_refcount>0);
    return (--m_refcount)==0; 
  }

  /**
   * Access refcount
   *
   * @return
   *   Current reference count
   */
  unsigned int
  count() const { return m_refcount; }
};

/**
 * Share ownership of a reference counted object
 *
 * This pointer class is used to retain ownership of a CL object.  When
 * the pointer object goes out of scope the object is released and if
 * the resulting reference count is zero, the object is deleted.  
 *
 * The pointer class is used in the core implementation to represent the
 * OpenCL object model.
 */
template <typename T>
class shared_ptr
{
  T* m_t;
public:
  typedef T* value_type;

  shared_ptr(T* t=nullptr) : m_t(t)
  {
    if (m_t)
      m_t->retain();
  }
  ~shared_ptr()
  {
    if (m_t && m_t->release())
      delete m_t;
  }
  
  shared_ptr(shared_ptr&& rhs)
    : m_t(rhs.m_t)
  {
    rhs.m_t = nullptr;
  }

  shared_ptr(const shared_ptr& rhs)
    : m_t(rhs.m_t)
  {
    if (m_t)
      m_t->retain();
  }

  shared_ptr& 
  operator= (shared_ptr rhs)
  {
    std::swap(m_t,rhs.m_t);
    return *this;
  }

  bool
  operator==(const T* rhs) const
  {
    return m_t == rhs;
  }

  bool 
  operator==(const shared_ptr& rhs) const
  {
    return m_t == rhs.m_t;
  }
  
  T* operator->() const
  {
    return m_t;
  }

  T* get() const
  {
    return m_t;
  }

  T* release()
  {
    T* ret = m_t;
    m_t = nullptr;
    return ret;
  }
};

template <typename T>
using ptr = shared_ptr<T>;

/**
 * The ptr_iterator overrides operator* to unwrap the ptr
 *
 * For use with containers of ptr<T> when iteration should
 * not retain ownership.
 *
 * \code
 *  typedef std::vector<ptr<device>> vec_type;
 *  typedef ptr_iterator<typename vec_type::iterator> iterator;
 *  auto dr = range<terator>(vec.begin(),vec.end());
 *  device* dev = *(dr.begin());
 * \code
 *
 * This iterator is in particular relied upon when ranges are 
 * assigned to a param_buffer in clGetXInfo() calls.
 */
template <typename Iterator>
struct ptr_iterator : public Iterator
{
  using iterator_value_type = typename std::iterator_traits<Iterator>::value_type;
  using value_type = typename iterator_value_type::value_type;
  static_assert(std::is_pointer<value_type>::value,"Only pointer type support");

  ptr_iterator(Iterator itr)
    : Iterator(itr)
  {}

  value_type operator*()
  {
    auto& p = static_cast<Iterator*>(this)->operator*();
    return p.get();
  }
};

} // xocl

#endif


