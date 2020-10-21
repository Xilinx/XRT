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

#ifndef xocl_core_param_h_
#define xocl_core_param_h_

#include "xocl/core/object.h"
#include "xocl/core/error.h"
#include "xocl/core/range.h"
#include "xrt/util/td.h"
#include <vector>
#include <string>
#include <cstring>

namespace xocl {

/**
 * Wrap a value buffer used by CL param query functions
 *
 * This wrapper encapsulates much of the error checking
 * associated with param query
 */
class param_buffer
{
  struct buffer
  {
    void* m_buffer;    // user supplied buffer to write to
    size_t m_size;     // size of buffer

    buffer(void* buffer, size_t size)
      : m_buffer(buffer), m_size(size)
    {}
  };

  buffer m_buffer;    // combine user supplied values
  size_t* m_size_ret; // actual size of data requested

  // Convert user buffer to type
  template <typename T>
  struct allocator
  {
    static T*
    get(buffer& b, size_t sz)
    {
      if (!b.m_buffer)
        return nullptr;

      if (b.m_buffer && b.m_size < sz * sizeof(T))
        throw error(CL_INVALID_VALUE, "Insufficient param value size");
      auto buffer = static_cast<T*>(b.m_buffer);
      b.m_buffer = (buffer + sz);
      b.m_size -= sz * sizeof(T);
      return buffer;
    }
  };

  // Assignment from type S to type T
  template <typename T>
  struct assignee
  {
    typedef T value_type;
    param_buffer& m_host;

    // implicit
    assignee(param_buffer& pb)
      : m_host(pb)
    {}

    // Default write for source type S
    // param.as<int>() = 5;
    template <typename S, int dummy=0>
    struct writer
    {
      template <typename SS, bool is_scalar>
      struct writer_helper;

      template <typename SS>
      struct writer_helper<SS,true>
      {
        static size_t
        write(buffer& b, SS s)
        {
          auto buffer = allocator<T>::get(b,1);
          if (buffer)
            *buffer = s;
          return sizeof(T);
        }
      };

      static size_t
      write(buffer& b, S t)
      {
        return writer_helper<T,std::is_scalar<T>::value>::write(b,t);
      }
    };

    // Specialize for array type
    // param.as<char>() = "hello";
    template <typename S, size_t N, int dummy>
    struct writer<S[N],dummy>
    {
      static size_t
      write(buffer& b, const S s[N])
      {
        return writer<T>::write(b,s);
      }
    };

    // Specialize for c-style string
    // param.as<char>() = static_cast<const char*>("hello");
    // param.as<unsigned char>() = static_cast<const char*>("hello");
    template <int dummy>
    struct writer<char,dummy>
    {
      static_assert(std::is_convertible<char,T>::value,"type mismatch: cannot convert char to T");

      static size_t
      write(buffer& b, const char* str)
      {
        size_t l = std::strlen(str);
        auto buffer = allocator<T>::get(b,l+1);
        if (buffer)
          std::copy(str,str+l+1,buffer);
        return l+1;
      }
    };

    // Specialize for std::string
    // param.as<char>() = std::string("hello");
    // param.as<unsigned char>() = std::string("hello");
    template <int dummy>
    struct writer<std::string,dummy>
    {
      static_assert(std::is_convertible<char,T>::value,"type mismatch: cannot convert char to T");

      static size_t
      write(buffer& b, const std::string& str)
      {
        size_t l = str.length();
        auto buffer = allocator<T>::get(b,l+1);
        if (buffer) {
          auto cstr = str.c_str();
          std::copy(cstr,cstr+l+1,buffer);
        }
        return l+1;
      }
    };

    // Specialize for vector type
    // std::vector<int> vec = {1,2,3,4};
    // param.as<int>() = vec;
    template <typename S,int dummy>
    struct writer<std::vector<S>,dummy>
    {
      typedef typename std::vector<S>::value_type value_type;
      static_assert(std::is_scalar<value_type>::value,"only scalar type supported");
      static_assert(std::is_convertible<value_type,T>::value,"type mismatch: cannot convert vector value type to T");

      static size_t
      write(buffer& b, const std::vector<S>& vec)
      {
        size_t l = vec.size();
        auto buffer = allocator<T>::get(b,l);
        if (buffer)
          std::copy(vec.begin(),vec.end(),buffer);
        return l*sizeof(T);
      }
    };

    // Specialize for range (bidirectional)
    // std::vector<int> vec = {1,2,3,4};
    // param.as<int>() = range(vec);
    template <typename BidirectionalIterator,int dummy>
    struct writer<range<BidirectionalIterator>,dummy>
    {
      typedef typename range<BidirectionalIterator>::value_type value_type;
      static_assert(std::is_scalar<value_type>::value,"only scalar type supported");
      static_assert(std::is_convertible<value_type,T>::value,"type mismatch: cannot convert range value type to T");

      static size_t
      write(buffer& b, const range<BidirectionalIterator>& r)
      {
        size_t l = r.size();
        auto buffer = allocator<T>::get(b,l);
        if (buffer) {
#ifndef _WIN32
          std::copy(r.begin(),r.end(),buffer);
#else
          for (auto v : r)
            *buffer++ = v;
#endif
        }
        return l*sizeof(T);
      }
    };

    // Primary assignment interface
    template<typename S>
    param_buffer&
    operator=(const S& s)
    {
      size_t sz = writer<S>::write(m_host.m_buffer,s);
      if (m_host.m_size_ret)
        *(m_host.m_size_ret) += sz;
      return m_host;
    }
  };

public:

  param_buffer(void* buffer, size_t size, size_t* size_ret)
    : m_buffer(buffer,size), m_size_ret(size_ret)
  {
    if (m_size_ret)
      *m_size_ret = 0;
  }


  /**
   * Assign a value of type T to the param_buffer.
   *
   * \code
   *  param_buffer param { buffer, sizeof(int)*1, &sz };
   *  param.as<int> = 5;
   * \endcode
   */
  template <typename T>
  assignee<T>
  as()
  {
    return assignee<T>(*this);
  }

  /**
   * Allocate an array of type T from the param buffer storage
   *
   * Example:
   * \code
   *  size_t sz = 0;
   *  char** stuff = (char**) malloc(4 * sizeof(char*));
   *  for (auto i : {0,1,2,3}) {
   *   stuff[i] = (char*)malloc(10);
   *  }
   *  param_buffer param { stuff, 4 * sizeof(char*), &sz };
   *  char** buf1  = param.as_array<char*>(1);
   *  char** buf2  = param.as_array<char*>(1);
   *  char** buf34 = param.as_array<char*>(2);
   *  char* buf3 = buf34[0];
   *  char* buf4 = buf34[1];
   * \endcode
   *
   * @param sz
   *   Number of elements in return array
   * @return
   *   Array of type T with sz elements.
   */
  template <typename T>
  T*
  as_array(size_t sz)
  {
    T* buffer = allocator<T>::get(m_buffer,sz);
    if (m_size_ret)
      *m_size_ret += sizeof(T) * sz;
    return buffer;
  }
};

} // xocl

#endif
