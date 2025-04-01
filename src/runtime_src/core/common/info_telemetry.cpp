// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
#define XRT_CORE_COMMON_SOURCE
#include "info_telemetry.h"
#include "query_requests.h"

#include <boost/format.hpp>
#include <vector>

namespace {
template <typename IntegralType>
bool 
is_value_na(IntegralType value) 
{
  return value == std::numeric_limits<IntegralType>::max();
}

static void
add_rtos_tasks(const xrt_core::device* device, boost::property_tree::ptree& pt)
{
  const auto data = xrt_core::device_query<xrt_core::query::rtos_telemetry>(device);
  boost::property_tree::ptree pt_rtos_array;
  for (const auto& rtos_task : data) {
    boost::property_tree::ptree pt_rtos_inst;
    if(is_value_na(rtos_task.context_starts)) 
      return; //not supported
    pt_rtos_inst.put("started_count", rtos_task.context_starts);
    pt_rtos_inst.put("scheduled_count", rtos_task.schedules);
    pt_rtos_inst.put("syscall_count", rtos_task.syscalls);
    pt_rtos_inst.put("dma_access_count", rtos_task.dma_access);
    pt_rtos_inst.put("resource_acquisition_count", rtos_task.resource_acquisition);

    boost::property_tree::ptree pt_dtlbs;
    for (const auto& dtlb : rtos_task.dtlbs) {
      boost::property_tree::ptree pt_dtlb;
      if(is_value_na(dtlb.misses)) 
        return; //not supported
      pt_dtlb.put("dtlb_misses", dtlb.misses);
      pt_dtlbs.push_back({"", pt_dtlb});
    }
    pt_rtos_inst.add_child("dtlb_data", pt_dtlbs);

    pt_rtos_array.push_back({"", pt_rtos_inst});
  }
  pt.add_child("rtos_tasks", pt_rtos_array);
}

static boost::property_tree::ptree
aie2_preemption_info(const xrt_core::device* device)
{
  const auto data = xrt_core::device_query<xrt_core::query::rtos_telemetry>(device);
  boost::property_tree::ptree pt_rtos_array;
  int user_task = 0;

  for (const auto& kp : data) {
  boost::property_tree::ptree pt_preempt;

  auto populate_value = [](uint64_t value) {
    return is_value_na(value) ? "N/A" : std::to_string(value);
  };

  pt_preempt.put("fw_tid", user_task++);
  pt_preempt.put("ctx_index", populate_value(kp.preemption_data.slot_index));
  pt_preempt.put("layer_events", populate_value(kp.preemption_data.preemption_checkpoint_event));
  pt_preempt.put("frame_events", populate_value(kp.preemption_data.preemption_frame_boundary_events));

  pt_rtos_array.push_back({"", pt_preempt});
  }
  return pt_rtos_array;
}

static void
add_opcode_info(const xrt_core::device* device, boost::property_tree::ptree& pt)
{
  const auto opcode_telem = xrt_core::device_query<xrt_core::query::opcode_telemetry>(device);

  boost::property_tree::ptree pt_opcodes;
  for (const auto& opcode : opcode_telem) {
    boost::property_tree::ptree pt_opcode;
    if(is_value_na(opcode.count)) 
      return; //not supported
    pt_opcode.put("received_count", opcode.count);
    pt_opcodes.push_back({"", pt_opcode});
  }
  pt.add_child("opcodes", pt_opcodes);
}

static void
add_stream_buffer_info(const xrt_core::device* device, boost::property_tree::ptree& pt)
{
  const auto stream_buffer_telem = xrt_core::device_query<xrt_core::query::stream_buffer_telemetry>(device);
  boost::property_tree::ptree pt_stream_buffers;
  for (const auto& stream_buf : stream_buffer_telem) {
    boost::property_tree::ptree pt_stream_buffer;
    if(is_value_na(stream_buf.tokens))
      return; //not supported
    pt_stream_buffer.put("tokens", stream_buf.tokens);
    pt_stream_buffers.push_back({"", pt_stream_buffer});
  }
  pt.add_child("stream_buffers", pt_stream_buffers);
}

static void
add_aie_info(const xrt_core::device* device, boost::property_tree::ptree& pt)
{
  const auto aie_telem = xrt_core::device_query<xrt_core::query::aie_telemetry>(device);
  boost::property_tree::ptree pt_aie_cols;
  for (const auto& aie_col : aie_telem) {
    boost::property_tree::ptree pt_aie_col;
    if(is_value_na(aie_col.deep_sleep_count))
      return; //not supported
    pt_aie_col.put("deep_sleep_count", aie_col.deep_sleep_count);
    pt_aie_cols.push_back({"", pt_aie_col});
  }
  pt.add_child("aie_columns", pt_aie_cols);
}

static boost::property_tree::ptree
aie2_telemetry_info(const xrt_core::device* device)
{
  boost::property_tree::ptree pt;

  try {
    const auto misc_telem = xrt_core::device_query<xrt_core::query::misc_telemetry>(device);
    if(!is_value_na(misc_telem.l1_interrupts))
      pt.put("level_one_interrupt_count", misc_telem.l1_interrupts);

    add_rtos_tasks(device, pt);
    add_opcode_info(device, pt);
    add_stream_buffer_info(device, pt);
    add_aie_info(device, pt);
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

boost::property_tree::ptree
preemption_telemetry_info(const xrt_core::device* device)
{
  boost::property_tree::ptree telemetry_pt;
  const auto device_class = xrt_core::device_query_default<xrt_core::query::device_class>(device, xrt_core::query::device_class::type::alveo);
  switch (device_class) {
  case xrt_core::query::device_class::type::alveo: // No telemetry for alveo devices
    return telemetry_pt;
  case xrt_core::query::device_class::type::ryzen:
  {
    telemetry_pt.add_child("telemetry", aie2_preemption_info(device));
    return telemetry_pt;
  }
  }
  return telemetry_pt;
}

}} // telemetry, xrt
