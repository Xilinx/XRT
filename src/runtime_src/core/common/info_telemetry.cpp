/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
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
#define XRT_CORE_COMMON_SOURCE
#include "info_telemetry.h"
#include "query_requests.h"

#include <boost/algorithm/string.hpp>

// Too much typing
namespace xq = xrt_core::query;

namespace xrt_core::telemetry {

boost::property_tree::ptree
telemetry_info(const xrt_core::device * device)
{
  boost::property_tree::ptree pt_root;
  std::vector<char> telemetry_raw;
  try {
    telemetry_raw = xrt_core::device_query<xq::telemetry>(device);
  }
  catch(xq::exception &) {
    // Telemetry not available
    pt_root.put("error_msg", "Telemetry data is not available");
    //return pt_root;
  }

  boost::property_tree::ptree pt_telemetry_array;
  xq::telemetry::data_type* telemetry_info = reinterpret_cast<xq::telemetry::data_type*>(telemetry_raw.data());
  boost::ignore_unused(telemetry_info);

  if(1) {
//   if(telemetry_info->piece1.counter_ops != -1) { //to-do: understand how to access members
    boost::property_tree::ptree pt_stat;
    pt_stat.add("label", "counter ops");
    pt_stat.add("value", 1);
    // pt_stat.add("value", telemetry_info.piece1.counter_ops);
    pt_telemetry_array.push_back(std::make_pair("", pt_stat));
  }

  pt_root.put_child("telemetry", pt_telemetry_array);
  return pt_root;
}
}

//   union telemetry_data {
//     struct piece1 {
//       uint32_t counter_ops[2] ;
//       uint32_t context_starting[10];
//       uint32_t scheduler_scheduled[10];
//       uint32_t syscalls[10];
//       uint32_t did_dma[10];
//       uint32_t resource_acquired[10];
//       uint32_t sb_tokens[4];
//       uint32_t deep_slp[6];
//       uint32_t trace_opcode[30];
//     };
//     struct piece2 {
//         uint32_t dtlb_misses[10][11];
//     };
//   };