/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef xrt_core_common_query_h
#define xrt_core_common_query_h

#include <boost/any.hpp>
#include <boost/format.hpp>

#include <stdexcept>

namespace xrt_core {

class device;

/**
 * query request infrastructure
 */
namespace query {

enum class key_type;

/**
 * class request - virtual dispatch to concrete query requests
 *
 * The queue_type hierarchy is
 *
 *       [request]
 *           ^
 *           |
 *     [<key_type>]
 *           ^
 *           |
 *    [implementation]
 *
 * The middle layer declares types and helper functions for a specify
 * query type.  The middle layer is public, defined in this file, and
 * is used as template argument to query function calls.
 *
 * The implementation layer defines one of the dispatch functions to
 * implement the query request itself.
 *
 * auto device* = ...;
 * auto vendor = device_query<pcie_vendor>(device);
 * auto bdf = device_query<pcie_bdf>(device);
 * auto bdf_string = pcie_bdf::to_string(bdf);
 */
struct request
{
  // Modifier for specific request accessor.   For some
  // query::request types, the accessor can expand into
  // multiple different requests at run-time. For example
  // when accessing sysfs nodes, the actual node can be
  // parameterized by modifying the hardware subdev or entry.
  enum class modifier { subdev, entry };

  virtual
  ~request()
  {}
  
  virtual boost::any
  get(const device*) const
  { throw std::runtime_error("query request requires arguments"); }

  virtual boost::any
  get(const device*, const boost::any&) const
  { throw std::runtime_error("query request does not support one argument"); }

  virtual boost::any
  get(const device*, const boost::any&, const boost::any&) const
  { throw std::runtime_error("query does not support two arguments"); }

  virtual boost::any
  get(const device*, modifier, const std::string&) const
  { throw std::runtime_error("query does not support modifier"); }

  virtual boost::any
  get(const device*, const boost::any&, const boost::any&, const boost::any&) const
  { throw std::runtime_error("query does not support three argunents"); }

  virtual void
  put(const device*, const boost::any&) const
  { throw std::runtime_error("query update does not support one argument"); }
};

// Base class for query exceptions.
//
// Provides granularity for calling code to catch errors specific to
// query request which are often acceptable errors because some
// devices may not support all types of query requests.
//
// Other non query exceptions signal a different kind of error which
// should maybe not be caught.
//
// The addition of the query request exception hierarchy does not
// break existing code that catches std::exception (or all errors)
// because ultimately the base query exception is-a std::exception
class exception : public std::runtime_error
{
public:
  explicit
  exception(const std::string& err)
    : std::runtime_error(err)
  { /*empty*/ }
};

class no_such_key : public exception
{
  key_type m_key;

  using qtype = std::underlying_type<query::key_type>::type;
public:
  explicit
  no_such_key(key_type k)
    : exception(boost::str(boost::format("No such query request (%d)") % static_cast<qtype>(k)))
    , m_key(k)
  {}

  no_such_key(key_type k, const std::string& msg)
    : exception(msg)
    , m_key(k)
  {}

  key_type
  get_key() const
  {
    return m_key;
  }
};

class sysfs_error : public exception
{
public:
  explicit
  sysfs_error(const std::string& msg)
    : exception(msg)
  { /*empty*/ }
};

class not_supported : public exception
{
public:
  explicit
  not_supported(const std::string& msg)
    : exception(msg)
  { /*empty*/ }
};

} // query

} // xrt_core
#endif
