// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "ReportTelemetry.h"

#include "core/common/info_telemetry.h"
#include "tools/common/Table2D.h"

#include <vector>

const boost::property_tree::ptree empty_ptree;

void
ReportTelemetry::getPropertyTreeInternal(const xrt_core::device* dev,
                                         boost::property_tree::ptree& pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void
ReportTelemetry::getPropertyTree20202(const xrt_core::device* dev,
                                      boost::property_tree::ptree& pt) const
{
  // There can only be 1 root node
  pt = xrt_core::telemetry::telemetry_info(dev);
}

static std::string
generate_rtos_dtlb_string(const boost::property_tree::ptree& pt)
{
  std::stringstream ss;

  boost::property_tree::ptree rtos_tasks = pt.get_child("rtos_tasks", empty_ptree);
  boost::property_tree::ptree rtos_dtlb_data = pt.get_child("rtos_tasks.dtlb_data", empty_ptree);
  if(rtos_tasks.empty() && rtos_dtlb_data.empty())
    return ss.str();

  std::vector<Table2D::HeaderData> dtlb_headers = {
    {"RTOS Task", Table2D::Justification::left}
  };

  // Create the headers for the DTLB table
  for (size_t i = 0; i < rtos_dtlb_data.size(); i++) {
    const std::string regionHeader = boost::str(boost::format("Region %d Misses") % std::to_string(i));
    dtlb_headers.push_back({regionHeader, Table2D::Justification::left});
  }
  Table2D rtos_dtlb_table(dtlb_headers);

  int index = 0;
  for (const auto& [name, rtos_task] : rtos_tasks) {
    std::vector<std::string> dtlb_data = {
      std::to_string(index)
    };

    boost::property_tree::ptree task_dtlbs = rtos_task.get_child("dtlb_data", empty_ptree);
    for (const auto& [dtlb_name, dtlb] : task_dtlbs) {
      dtlb_data.push_back(std::to_string(dtlb.get<uint64_t>("dtlb_misses")));
    }
    rtos_dtlb_table.addEntry(dtlb_data);

    index++;
  }

  ss << "  RTOS DTLBs\n";
  ss << rtos_dtlb_table.toString("  ") << "\n";

  return ss.str();
}

static std::string
generate_rtos_string(const boost::property_tree::ptree& pt)
{
  std::stringstream ss;

  boost::property_tree::ptree rtos_tasks = pt.get_child("rtos_tasks", empty_ptree);
  if(rtos_tasks.empty())
    return ss.str();

  const std::vector<Table2D::HeaderData> rtos_headers = {
    {"RTOS Task", Table2D::Justification::left},
    {"Starts", Table2D::Justification::left},
    {"Schedules", Table2D::Justification::left},
    {"Syscalls", Table2D::Justification::left},
    {"DMA Accesses", Table2D::Justification::left},
    {"Resource Acquisitions", Table2D::Justification::left},
  };
  Table2D rtos_table(rtos_headers);

  int index = 0;
  for (const auto& [name, rtos_task] : rtos_tasks) {
    const std::vector<std::string> rtos_data = {
      std::to_string(index),
      std::to_string(rtos_task.get<uint64_t>("started_count")),
      std::to_string(rtos_task.get<uint64_t>("scheduled_count")),
      std::to_string(rtos_task.get<uint64_t>("syscall_count")),
      std::to_string(rtos_task.get<uint64_t>("dma_access_count")),
      std::to_string(rtos_task.get<uint64_t>("resource_acquisition_count")),
    };
    rtos_table.addEntry(rtos_data);

    index++;
  }

  ss << rtos_table.toString("  ") << "\n";

  return ss.str();
}

static std::string
generate_opcode_string(const boost::property_tree::ptree& pt)
{
  std::stringstream ss;

  const std::vector<Table2D::HeaderData> opcode_headers = {
    {"Mailbox Opcode", Table2D::Justification::left},
    {"Count", Table2D::Justification::left}
  };
  Table2D opcode_table(opcode_headers);

  int index = 0;
  boost::property_tree::ptree opcodes = pt.get_child("opcodes", empty_ptree);
  if(opcodes.empty())
    return ss.str();
  for (const auto& [name, opcode] : opcodes) {
    std::vector<std::string> opcode_data = {
      std::to_string(index),
      std::to_string(opcode.get<uint64_t>("received_count"))
    };
    opcode_table.addEntry(opcode_data);
    index++;
  }

  ss << opcode_table.toString("  ") << "\n";

  return ss.str();
}

static std::string
generate_stream_buffer_string(const boost::property_tree::ptree& pt)
{
  std::stringstream ss;

  const std::vector<Table2D::HeaderData> stream_buffer_headers = {
    {"Stream Buffer", Table2D::Justification::left},
    {"Tokens", Table2D::Justification::left}
  };
  Table2D stream_buffer_table(stream_buffer_headers);

  int index = 0;
  boost::property_tree::ptree stream_buffers = pt.get_child("stream_buffers", empty_ptree);
  if(stream_buffers.empty())
      return ss.str();

  for (const auto& [name, stream_buffer] : stream_buffers) {
    std::vector<std::string> stream_buffer_data = {
      std::to_string(index),
      std::to_string(stream_buffer.get<uint64_t>("tokens"))
    };
    stream_buffer_table.addEntry(stream_buffer_data);
    index++;
  }

  ss << stream_buffer_table.toString("  ") << "\n";

  return ss.str();
}

static std::string
generate_aie_string(const boost::property_tree::ptree& pt)
{
  std::stringstream ss;

  const std::vector<Table2D::HeaderData> aie_headers = {
    {"AIE Column", Table2D::Justification::left},
    {"Deep Sleep Count", Table2D::Justification::left}
  };
  Table2D aie_table(aie_headers);

  int index = 0;
  boost::property_tree::ptree aie_cols = pt.get_child("aie_columns", empty_ptree);
  if(aie_cols.empty())
    return ss.str();
  for (const auto& [name, aie_col] : aie_cols) {
    std::vector<std::string> aie_data = {
      std::to_string(index),
      std::to_string(aie_col.get<uint64_t>("deep_sleep_count"))
    };
    aie_table.addEntry(aie_data);
    index++;
  }

  ss << aie_table.toString("  ") << "\n";

  return ss.str();
}

static std::string
generate_misc_string(const boost::property_tree::ptree& pt)
{
  std::stringstream ss;
  auto l1_int = pt.get<std::string>("level_one_interrupt_count", "");

  if(!l1_int.empty())
    ss << boost::format("  %-23s: %s \n") % "L1 Interrupt Count" % l1_int << "\n";

  return ss.str();
}

void
ReportTelemetry::writeReport(const xrt_core::device* /*_pDevice*/,
                             const boost::property_tree::ptree& pt,
                             const std::vector<std::string>& /*_elementsFilter*/,
                             std::ostream& _output) const
{
  _output << "Telemetry\n";

  boost::property_tree::ptree telemetry_pt = pt.get_child("telemetry", empty_ptree);
  if (telemetry_pt.empty()) {
    _output << "  No telemetry information available\n\n";
    return;
  }

  _output << generate_misc_string(telemetry_pt);
  _output << generate_rtos_string(telemetry_pt);
  _output << generate_rtos_dtlb_string(telemetry_pt);
  _output << generate_opcode_string(telemetry_pt);
  _output << generate_stream_buffer_string(telemetry_pt);
  _output << generate_aie_string(telemetry_pt);
  _output << std::endl;
}
