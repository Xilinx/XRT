// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
#include "device_noop.h"
#include "shim.h"

#include "core/common/query_requests.h"

#include <string>

namespace {

using key_type = xrt_core::query::key_type;

struct kds_cu_info
{
  using result_type = xrt_core::query::kds_cu_info::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    return userpf::kds_cu_info(device);
  }
};

struct xclbin_slots
{
  using result_type = xrt_core::query::xclbin_slots::result_type;

  static result_type
  get(const xrt_core::device* device, key_type)
  {
    return userpf::xclbin_slots(device);
  }
};

static std::map<xrt_core::query::key_type, std::unique_ptr<xrt_core::query::request>> query_tbl;

template <typename QueryRequestType, typename Getter>
struct function0_getter : QueryRequestType
{
  static_assert(std::is_same<typename Getter::result_type, typename QueryRequestType::result_type>::value
             || std::is_same<typename Getter::result_type, std::any>::value, "type mismatch");

  std::any
  get(const xrt_core::device* device) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k);
  }
};

template <typename QueryRequestType, typename Getter>
static void
emplace_function0_getter()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function0_getter<QueryRequestType, Getter>>());
}

static void
initialize_query_table()
{
  emplace_function0_getter<xrt_core::query::kds_cu_info,               kds_cu_info>();
  emplace_function0_getter<xrt_core::query::xclbin_slots,              xclbin_slots>();
}

struct X { X() { initialize_query_table(); }};
static X x;

}

namespace xrt_core { namespace noop {

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

}} // noop,xrt_core
