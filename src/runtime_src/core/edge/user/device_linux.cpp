/**
 * Copyright (C) 2020 Xilinx, Inc
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


#include "device_linux.h"
#include "core/common/query_requests.h"

#include "xrt.h"

#include <string>
#include <memory>
#include <iostream>
#include <map>
#include <boost/format.hpp>

namespace {

namespace query = xrt_core::query;
using key_type = query::key_type;

static std::map<query::key_type, std::unique_ptr<query::request>> query_tbl;

}

namespace xrt_core {

const query::request&
device_linux::
lookup_query(query::key_type query_key) const
{
  auto it = query_tbl.find(query_key);

  if (it == query_tbl.end()) {
    using qtype = std::underlying_type<query::key_type>::type;
    std::string err = boost::str( boost::format("The given query request ID (%d) is not supported on Edge Linux.")
                                  % static_cast<qtype>(query_key));
    throw std::runtime_error(err);
  }

  return *(it->second);
}

device_linux::
device_linux(id_type device_id, bool user)
  : shim<device_edge>(device_id, user)
{
}

device_linux::
device_linux(handle_type device_handle, id_type device_id)
  : shim<device_edge>(device_handle, device_id)
{
}

void
device_linux::
read_dma_stats(boost::property_tree::ptree& pt) const
{
}

void
device_linux::
read(uint64_t offset, void* buf, uint64_t len) const
{

  throw error(-ENODEV, "read failed");
}

void
device_linux::
write(uint64_t offset, const void* buf, uint64_t len) const
{
  throw error(-ENODEV, "write failed");
}

} // xrt_core
