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
    xclswemuhal2::SwEmuShim *drv = xclswemuhal2::SwEmuShim::handleCheck(device->get_device_handle());
    if (!drv)
      return 0;
    return drv->deviceQuery(query_key);
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

std::unique_ptr<buffer_handle>
device::
import_bo(pid_t pid, shared_handle::export_handle ehdl)
{
  if (pid == 0 || getpid() == pid)
    return xrt::shim_int::import_bo(get_device_handle(), ehdl);

  throw xrt_core::error(std::errc::not_supported, __func__);
}

void
device::
get_device_info(xclDeviceInfo2 *info)
{
  if (auto ret = xclGetDeviceInfo2(get_device_handle(), info))
    throw system_error(ret, "failed to get device info");
}

std::unique_ptr<xrt_core::graph_handle>
device::
open_graph_handle(const xrt::uuid& xclbin_id, const char* name, xrt::graph::access_mode am)
{
   return std::make_unique<xclswemuhal2::SwEmuShim::graph_object>(
                  static_cast<xclswemuhal2::SwEmuShim*>(get_device_handle()), xclbin_id, name, am);
}

void
device::
open_aie_context(xrt::aie::access_mode am)
{
 if (auto ret = xclAIEOpenContext(get_device_handle(), am))
   throw error(ret, "fail to open aie context");
}

void
device::
sync_aie_bo(xrt::bo& bo, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (auto ret = xclSyncBOAIE(get_device_handle(), bo, gmioName, dir, size, offset))
    throw system_error(ret, "fail to sync aie bo");
}

void
device::
reset_aie()
{
  if (auto ret = xclResetAIEArray(get_device_handle()))
    throw system_error(ret, "fail to reset aie");
}

void
device::
sync_aie_bo_nb(xrt::bo& bo, const char *gmioName, xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (auto ret = xclSyncBOAIENB(get_device_handle(), bo, gmioName, dir, size, offset))
    throw system_error(ret, "fail to sync aie non-blocking bo");
}

void
device::
wait_gmio(const char *gmioName)
{
  if (auto ret = xclGMIOWait(get_device_handle(), gmioName))
    throw system_error(ret, "fail to wait gmio");
}

int
device::
start_profiling(int option, const char* port1Name, const char* port2Name, uint32_t value)
{
  return xclStartProfiling(get_device_handle(), option, port1Name, port2Name, value);
}

uint64_t
device::
read_profiling(int phdl)
{
  return xclReadProfiling(get_device_handle(), phdl);
}

void
device::
stop_profiling(int phdl)
{
  if (auto ret = xclStopProfiling(get_device_handle(), phdl))
    throw system_error(ret, "failed to stop profiling");
}

void
device::
load_axlf_meta(const axlf* buffer)
{
  if (auto ret = xclLoadXclBinMeta(get_device_handle(), buffer))
    throw system_error(ret, "failed to load xclbin");
}

}} // swemu,xrt_core
