#ifndef xrt_core_common_query_h
#define xrt_core_common_query_h

#include <stdexcept>
#include <boost/any.hpp>

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

  virtual void
  put(const device*, const boost::any&) const
  { throw std::runtime_error("query update does not support one argument"); }
};

} // query

} // xrt_core
#endif
