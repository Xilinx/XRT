// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "info_telemetry.h"
#include "query_requests.h"

#include <boost/format.hpp>
#include <vector>

namespace {

static void
add_opcode_info(const xrt_core::device* device, boost::property_tree::ptree& pt)
{
  const auto opcode_telem = xrt_core::device_query<xrt_core::query::opcode_telemetry>(device);

  boost::property_tree::ptree pt_opcodes;
  for (const auto& opcode : opcode_telem) {
    boost::property_tree::ptree pt_opcode;
    pt_opcode.put("received_count", opcode.count);
    pt_opcodes.push_back({"", pt_opcode});
  }
  pt.add_child("opcodes", pt_opcodes);
}

static boost::property_tree::ptree
aie2_telemetry_info(const xrt_core::device* device)
{
  boost::property_tree::ptree pt;

  try {
    add_opcode_info(device, pt);
  }
  catch (const xrt_core::query::no_such_key&) {
    // Queries are not setup
    boost::property_tree::ptree empty_pt;
    return empty_pt;
  }
  catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
    return pt;
  }

  return pt;
}

} //unnamed namespace

namespace xrt_core { namespace telemetry {

boost::property_tree::ptree
telemetry_info(const xrt_core::device* device)
{
  boost::property_tree::ptree telemetry_pt;
  const auto device_class = xrt_core::device_query_default<xrt_core::query::device_class>(device, xrt_core::query::device_class::type::alveo);
  switch (device_class) {
  case xrt_core::query::device_class::type::alveo: // No telemetry for alveo devices
    return telemetry_pt;
  case xrt_core::query::device_class::type::ryzen:
  {
    telemetry_pt.add_child("telemetry", aie2_telemetry_info(device));
    return telemetry_pt;
  }
  }
  return telemetry_pt;
}

}} // telemetry, xrt
