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

#ifndef xocl_core_range_h_
#define xocl_core_range_h_

#include <boost/range/iterator_range.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/map.hpp>
#include <boost/range/join.hpp>
#include <vector>
#include <mutex>

namespace xocl {

template <typename IteratorT>
using range = boost::iterator_range<IteratorT>;

template <typename Range1, typename Range2>
using joined_range = boost::range::joined_range<Range1,Range2>;

template <typename StdPairRange>
using value_range = boost::select_second_const_range<StdPairRange>;


// Example of filtered range
//  filtered_range<argument_vector_type,argument_filter_type>
//  get_progvar_argument_range() const
//  {
//    argument_filter_type filter = [](const argument_value_type& arg) { return arg->is_progvar(); };
//    return boost::adaptors::filter(m_args, filter);
//  }
template <typename Range, typename Predicate>
using filtered_range = boost::filtered_range<Predicate,const Range>;

template <typename Container>
auto
get_range(const Container& c) -> range<typename Container::const_iterator>
{
  return range<typename Container::const_iterator>(c.begin(),c.end());
}

template <typename Container>
auto
get_range(Container& c) -> range<typename Container::iterator>
{
  return range<typename Container::iterator>(c.begin(),c.end());
}

template <typename Iterator>
range<Iterator>
get_range(Iterator begin, Iterator end)
{
  return range<Iterator>(begin,end);
}

/**
 * Locking range
 *
 * Lock to internal data structure is transferred to this range
 * and remains locked until range goes out of scope.
 */
template <typename Iterator>
class range_lock : public range<Iterator>
{
  std::unique_lock<std::mutex> m_lock;
public:
  range_lock(Iterator b, Iterator e, std::unique_lock<std::mutex>&& lock)
    : range<Iterator>(b,e), m_lock(std::move(lock))
  {}
};

/**
 * Locking range of a container
 *
 * Lock to internal data structure is transferred to this range
 * and remains locked until range goes out of scope.
 */
template <typename Range>
class range_zip_lock
{
  using value_type = typename Range::value_type;
  using container_type = std::vector<value_type>;
  using const_iterator = typename container_type::const_iterator;
  std::vector<value_type> m_r;  // can't get boost::joined_range to work as I want
  std::unique_lock<std::mutex> m_lock;
public:
  range_zip_lock(const Range& r1, const Range& r2, std::unique_lock<std::mutex>&& lock)
    : m_lock(std::move(lock))
  {
    std::copy(r1.begin(),r1.end(),std::back_inserter(m_r));
    std::copy(r2.begin(),r2.end(),std::back_inserter(m_r));
  }

  const_iterator
  begin() const 
  {
    return m_r.begin(); 
  }

  const_iterator
  end() const 
  {
    return m_r.end(); 
  }
};

template <typename Range, typename OutputIterator>
OutputIterator
range_copy(const Range& r, OutputIterator itr)
{
  return std::copy(r.begin(),r.end(),itr);
}

template <typename Range>
typename Range::const_iterator
range_find(const Range& r, typename Range::value_type v)
{
  return std::find(r.begin(),r.end(),v);
}

template <typename Range>
typename Range::iterator
range_find(Range& r, typename Range::value_type v)
{
  return std::find(r.begin(),r.end(),v);
}

template <typename Range,typename UnaryPredicate>
typename Range::const_iterator
range_find(const Range& r, UnaryPredicate p)
{
  return std::find_if(r.begin(),r.end(),p);
}

} // xocl

#endif


