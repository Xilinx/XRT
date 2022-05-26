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
#include "device_swemu.h"
#include "core/common/query_requests.h"
#include "core/pcie/emulation/common_em/query.h"
#include "shim.h"

#include <string>
#include <map>
#include <boost/format.hpp>

namespace {

namespace query = xrt_core::query;
using key_type = query::key_type;
using qtype = std::underlying_type<query::key_type>::type;

static std::map<query::key_type, std::unique_ptr<query::request>> query_tbl;

struct device_query
{  
  static uint32_t
    get(const xrt_core::device* device, key_type query_key)
  {
    xclcpuemhal2::CpuemShim *drv = xclcpuemhal2::CpuemShim::handleCheck(device->get_device_handle());
    if (!drv)
      return 0;
    return drv->deviceQuery(query_key);
  }
};


template <typename QueryRequestType, typename Getter>
struct function0_get : virtual QueryRequestType
{
  boost::any
    get(const xrt_core::device* device) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k);
  }
};

template <typename QueryRequestType, typename Getter>
static void
emplace_func0_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function0_get<QueryRequestType, Getter>>());
}

static void
initialize_query_table()
{
  emplace_func0_request<query::clock_freqs_mhz, xclemulation::query::device_info>();
  emplace_func0_request<query::kds_numcdmas, xclemulation::query::device_info>();
  emplace_func0_request<query::pcie_bdf, xclemulation::query::device_info>();
  emplace_func0_request<query::m2m, device_query>();
  emplace_func0_request<query::nodma, device_query>();
  emplace_func0_request<query::rom_vbnv, xclemulation::query::device_info>();
}

struct X { X() { initialize_query_table(); }};
static X x;

}

namespace xrt_core { namespace swemu {

const query::request&
device::
lookup_query(query::key_type query_key) const
{
  auto it = query_tbl.find(query_key);

  if (it == query_tbl.end())
    throw query::no_such_key(query_key);

  return *(it->second);
}

device::
device(handle_type device_handle, id_type device_id, bool user)
  : shim<device_pcie>(device_handle, device_id, user)
{
}

}} // swemu,xrt_core
