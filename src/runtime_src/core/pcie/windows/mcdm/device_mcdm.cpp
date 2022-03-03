// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc. All rights reserved.
#define XCL_DRIVER_DLL_EXPORT
#include "device_mcdm.h"
#include "core/common/query_requests.h"

namespace {

namespace query = xrt_core::query;
using key_type = xrt_core::query::key_type;
using qtype = std::underlying_type<query::key_type>::type;

static std::map<xrt_core::query::key_type, std::unique_ptr<xrt_core::query::request>> query_tbl;

static void
initialize_query_table()
{
}

struct X { X() { initialize_query_table(); }};
static X x;

}

namespace xrt_core {

const query::request&
device_mcdm::
lookup_query(query::key_type query_key) const
{
  auto it = query_tbl.find(query_key);

  if (it == query_tbl.end())
    throw query::no_such_key(query_key);

  return *(it->second);
}

device_mcdm::
device_mcdm(handle_type device_handle, id_type device_id, bool user)
  : shim<device_pcie>(user ? device_handle : nullptr, device_id, user)
{}

device_mcdm::
~device_mcdm()
{
}

} // xrt_core
