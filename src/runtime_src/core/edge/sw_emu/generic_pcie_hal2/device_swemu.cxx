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
#include <iostream>
#include <fstream>
#include "device_swemu.h"
#include "core/common/query_requests.h"

#include <string>
#include <map>
#include <boost/format.hpp>

namespace {

namespace query = xrt_core::query;
using key_type = query::key_type;
using qtype = std::underlying_type<query::key_type>::type;

static std::map<query::key_type, std::unique_ptr<query::request>> query_tbl;
struct board_name
{
  using result_type = query::board_name::result_type;

  static result_type
    get(const xrt_core::device* device, key_type)
  {
      result_type deviceName("edge");
      std::ifstream VBNV("/etc/xocl.txt");
      if (VBNV.is_open()) {
        VBNV >> deviceName;
      }
      VBNV.close();

      if (deviceName.empty()) {
        VBNV.open("platform_desc.txt");
        if (VBNV.is_open()) {
          VBNV >> deviceName;
        }
        VBNV.close();
      }

      return deviceName;
    }
};

template <typename QueryRequestType, typename Getter>
struct function0_get : virtual QueryRequestType
{
  std::any
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
  emplace_func0_request<query::rom_vbnv, board_name>();
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
  : shim<device_edge>(device_handle, device_id, user)
{
}

std::unique_ptr<buffer_handle>
device::
import_bo(pid_t pid, shared_handle::export_handle ehdl)
{
  if (pid == 0 || getpid() == pid)
    return xrt::shim_int::import_bo(get_device_handle(), ehdl);

  throw xrt_core::error(std::errc::not_supported, __func__);
}

}} // swemu,xrt_core
