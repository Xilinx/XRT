// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#include "device_hwemu.h"
#include "core/common/query_requests.h"
#include "core/pcie/emulation/common_em/query.h"
#include "shim.h"

#include <string>
#include <map>

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
    xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(device->get_device_handle());
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

struct X { X() { initialize_query_table(); } };
static X x;

}

namespace xrt_core { namespace hwemu {

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

}} // hwemu,xrt_core
