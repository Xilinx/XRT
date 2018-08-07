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

#ifndef xocl_core_property_h_
#define xocl_core_property_h_

#include "xocl/core/error.h"
#include "xocl/core/range.h"
#include "xrt/util/td.h"
#include <vector>
#include <set>
#include <string>
#include <cstring>

namespace xocl {

/**
 * Wrap a properties stored in OCL objects such such that the
 * properties can be easily tested and modified
 *
 * @param Rep
 *  The type of the underlying property values.  This type can
 *  currently only be a scalar type.
 */
template <typename Rep>
class property_object
{
  static_assert(std::is_scalar<Rep>::value,"only scalar type supported");
  Rep m_props;

public:

  property_object()
    : m_props(0)
  {}

  // implicit
  property_object(Rep props)
    : m_props(props)
  {}

  operator Rep () const
  {
    return m_props;
  }

  bool
  test(Rep rhs) const
  {
    return m_props & rhs;
  }

  property_object&
  operator |= (Rep rhs)
  {
    m_props |= rhs;
    return *this;
  }

  property_object&
  operator &= (Rep rhs)
  {
    m_props &= rhs;
    return *this;
  }
};

template <typename Rep>
class property_list
{
  struct element
  {
    Rep m_key;
    Rep m_value;

    element(Rep key, Rep value)
      : m_key(key), m_value(value)
    {}

    Rep
    get_key() const
    {
      return m_key;
    }

    Rep
    get_value() const
    {
      return m_value;
    }

    template <typename T>
    T get_value_as() const
    {
      return reinterpret_cast<T>(m_value);
    }

    bool operator< (const element& e) const
    {
      return m_key < e.m_key;
    }
  };

  using list_type = std::set<element>;
  using iterator = typename list_type::iterator;
  using const_iterator = typename list_type::const_iterator;
  list_type m_props;
public:

  property_list()
  {}

  property_list(const Rep* props)
  {
    std::set<Rep> keys;
    while (props && *props) {
      auto key = *props++;
      auto value = *props++;
      if (keys.count(key))
        throw xocl::error(CL_INVALID_PROPERTY,"same key specified twice");
      m_props.insert(element(key,value));
      keys.insert(key);
    }
  }

  const_iterator
  begin() const
  {
    return m_props.begin();
  }

  const_iterator
  end() const
  {
    return m_props.end();
  }

  template <typename T>
  T
  get_value_as(Rep key) const
  {
    auto elemkey = element(key,key); // bogus element to use as key
    auto keyitr = m_props.find(elemkey);
    return keyitr!=m_props.end()
      ? reinterpret_cast<T>((*keyitr).m_value)
      : nullptr;
  }
};

} // xocl

#endif
