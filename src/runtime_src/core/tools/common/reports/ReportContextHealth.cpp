// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportContextHealth.h"
#include "core/common/query_requests.h"
#include "core/common/time.h"
#include "tools/common/SmiWatchMode.h"
#include "tools/common/Table2D.h"

// 3rd Party Library - Include Files
#include <algorithm>
#include <boost/format.hpp>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>
#include <memory>

using bpt = boost::property_tree::ptree;
using context_health_info = xrt_core::query::context_health_info;

std::unique_ptr<ReportContextHealth>
ReportContextHealth::
create_reporter(xrt_core::smi::smi_hardware_config::hardware_type hw_type) const
{
  switch (hw_type)
  {
  case xrt_core::smi::smi_hardware_config::hardware_type::npu3_f1:
  case xrt_core::smi::smi_hardware_config::hardware_type::npu3_f2:
  case xrt_core::smi::smi_hardware_config::hardware_type::npu3_f3:
  case xrt_core::smi::smi_hardware_config::hardware_type::npu3_B01:
  case xrt_core::smi::smi_hardware_config::hardware_type::npu3_B02:
  case xrt_core::smi::smi_hardware_config::hardware_type::npu3_B03:
    return std::make_unique<ctx_health_npu3>();
  
  case xrt_core::smi::smi_hardware_config::hardware_type::stxA0:
  case xrt_core::smi::smi_hardware_config::hardware_type::stxB0:
  case xrt_core::smi::smi_hardware_config::hardware_type::stxH:
  case xrt_core::smi::smi_hardware_config::hardware_type::krk1:
  case xrt_core::smi::smi_hardware_config::hardware_type::phx:
  default:
    return std::make_unique<ctx_health_strx>();
  }
}

void
ReportContextHealth::
getPropertyTreeInternal(const xrt_core::device* dev, bpt& pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void
ReportContextHealth::
getPropertyTree20202(const xrt_core::device* dev, bpt& pt) const
{
  bpt context_health_pt{};

  try {
    // For property tree generation, always get all contexts
    auto context_health_data = xrt_core::device_query<context_health_info>(dev);

    // Group contexts by PID
    std::map<uint64_t, std::vector<context_health_info::smi_context_health>> contexts_by_pid;
    for (const auto& context : context_health_data) {
      contexts_by_pid[context.pid].push_back(context);
    }

    // Convert structured data to property tree, grouped by PID
    bpt pids_array{};
    for (const auto& [pid, contexts] : contexts_by_pid) {
      bpt pid_pt;
      pid_pt.put("pid", pid);
      
      bpt contexts_array{};
      for (const auto& context : contexts) {
        bpt context_pt;
        context_pt.put("ctx_id"                     ,context.ctx_id);
        const auto* health = reinterpret_cast<const ert_ctx_health_data_v1*>(context.health_data_raw.data());
        context_pt.put("txn_op_idx"                 ,health->aie2.txn_op_idx);
        context_pt.put("ctx_pc"                     ,health->aie2.ctx_pc);
        context_pt.put("fatal_error_type"           ,health->aie2.fatal_error_type);
        context_pt.put("fatal_error_exception_type" ,health->aie2.fatal_error_exception_type);
        context_pt.put("fatal_error_exception_pc"   ,health->aie2.fatal_error_exception_pc);
        context_pt.put("fatal_error_app_module"     ,health->aie2.fatal_error_app_module);
        contexts_array.push_back(std::make_pair("", context_pt));
      }
      pid_pt.add_child("contexts", contexts_array);
      pid_pt.put("context_count", contexts.size());
      
      pids_array.push_back(std::make_pair("", pid_pt));
    }
    context_health_pt.add_child("pids", pids_array);
    context_health_pt.put("total_context_count", context_health_data.size());
    context_health_pt.put("pid_count", contexts_by_pid.size());
  } 
  catch (const std::exception& e) {
    context_health_pt.put("context_count", 0);
    context_health_pt.put("error", e.what());
  }

  // There can only be 1 root node
  pt.add_child("context_health", context_health_pt);
}

std::vector<uint64_t>
ReportContextHealth::
parse_values(const std::string& input) const 
{
  std::vector<uint64_t> result;
  if (input.empty()) {
    return result;
  }
  
  std::stringstream ss(input);
  std::string token;
  
  while (std::getline(ss, token, ',')) {
    // Trim whitespace
    token.erase(0, token.find_first_not_of(" \t"));
    token.erase(token.find_last_not_of(" \t") + 1);
    
    if (!token.empty()) {
      try {
        uint64_t value = std::stoull(token);
        result.push_back(value);
      } 
      catch (const std::exception&) {
        // Skip invalid entries
      }
    }
  }
  
  return result;
}

std::vector<std::pair<uint64_t, uint64_t>>
ReportContextHealth::
parse_context_pid_pairs(const std::vector<std::string>& elements_filter) const 
{
  std::vector<uint64_t> context_ids;
  std::vector<uint64_t> pids;
  
  // Parse both ctx_id and pid from filter
  for (const auto& element : elements_filter) {
    if (element.find("ctx_id=") == 0) {
      std::string ctx_ids_str = element.substr(7); // Skip "ctx_id="
      context_ids = parse_values(ctx_ids_str);
    } 
    else if (element.find("pid=") == 0) {
      std::string pids_str = element.substr(4); // Skip "pid="
      pids = parse_values(pids_str);
    }
  }
  
  // Create pairs - map 1:1, pad with 0 if lists are different lengths
  std::vector<std::pair<uint64_t, uint64_t>> pairs;
  size_t max_size = std::max(context_ids.size(), pids.size());
  
  for (size_t i = 0; i < max_size; ++i) {
    uint64_t ctx_id = (i < context_ids.size()) ? context_ids[i] : 0;
    uint64_t pid = (i < pids.size()) ? pids[i] : 0;
    pairs.emplace_back(ctx_id, pid);
  }
  
  return pairs;
}

std::vector<uint64_t>
ReportContextHealth::
parse_context_ids(const std::vector<std::string>& elements_filter) const 
{
  for (const auto& element : elements_filter) {
    if (element.find("ctx_id=") == 0) {
      std::string ctx_ids_str = element.substr(7); // Skip "ctx_id="
      return parse_values(ctx_ids_str);
    }
  }
  return {};
}

std::string
ctx_health_strx::
generate_report(const xrt_core::device* dev,
                const std::vector<std::pair<uint64_t, uint64_t>>& context_pid_pairs,
                const std::vector<uint64_t>& context_ids) const
{
  std::stringstream ss;

  try {
    std::vector<context_health_info::smi_context_health> context_health_data;

    // If any pid is nonzero, pass pairs
    bool has_nonzero_pid = std::any_of(context_pid_pairs.begin(), context_pid_pairs.end(), [](const auto& p){ return p.second != 0; });
    if (!context_pid_pairs.empty() && has_nonzero_pid) {
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev, context_pid_pairs);
    } else if (!context_ids.empty()) {
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev, context_ids);
    } else {
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev);
    }

    auto context_count = context_health_data.size();

    if (context_count == 0) {
      ss << "No context health data available\n";
      return ss.str();
    }

    // Group contexts by PID
    std::map<uint64_t, std::vector<context_health_info::smi_context_health>> contexts_by_pid;
    for (const auto& context : context_health_data) {
      contexts_by_pid[context.pid].push_back(context);
    }

    for (const auto& [pid, contexts] : contexts_by_pid) {
      ss << "  Context Health Information (PID: " << pid << "):\n";

      const std::vector<Table2D::HeaderData> table_headers = {
        {"Ctx Id",               Table2D::Justification::left},
        {"Txn Op Idx",           Table2D::Justification::left},
        {"Ctx PC",               Table2D::Justification::left},
        {"Fatal Err Type",       Table2D::Justification::left},
        {"Fatal Err Ex Type",    Table2D::Justification::left},
        {"Fatal Err Ex PC",      Table2D::Justification::left},
        {"Fatal App Module",     Table2D::Justification::left}
      };
      Table2D context_table(table_headers);

      // Add data rows for this PID
      for (const auto& context : contexts) {
        const auto* health = reinterpret_cast<const ert_ctx_health_data_v1*>(context.health_data_raw.data());
        const std::vector<std::string> entry_data = {
          (boost::format("%d")   % context.ctx_id).str(),
          (boost::format("0x%x") % health->aie2.txn_op_idx).str(),
          (boost::format("0x%x") % health->aie2.ctx_pc).str(),
          (boost::format("0x%x") % health->aie2.fatal_error_type).str(),
          (boost::format("0x%x") % health->aie2.fatal_error_exception_type).str(),
          (boost::format("0x%x") % health->aie2.fatal_error_exception_pc).str(),
          (boost::format("0x%x") % health->aie2.fatal_error_app_module).str()
        };
        context_table.addEntry(entry_data);
      }

      ss << context_table.toString("    ");
      ss << "\n";
    } 
  }
  catch (const std::exception& e) {
    ss << "Error retrieving context health data: " << e.what() << "\n";
  }

  return ss.str();
}

std::string
ctx_health_npu3::
generate_report(const xrt_core::device* dev,
                const std::vector<std::pair<uint64_t, uint64_t>>& context_pid_pairs,
                const std::vector<uint64_t>& context_ids) const
{
  std::stringstream ss;

  try {
    std::vector<context_health_info::smi_context_health> context_health_data;

    // If any pid is nonzero, pass pairs
    bool has_nonzero_pid = std::any_of(context_pid_pairs.begin(), context_pid_pairs.end(), [](const auto& p){ return p.second != 0; });
    if (!context_pid_pairs.empty() && has_nonzero_pid) {
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev, context_pid_pairs);
    } else if (!context_ids.empty()) {
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev, context_ids);
    } else {
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev);
    }

    auto context_count = context_health_data.size();

    if (context_count == 0) {
      ss << "No context health data available\n";
      return ss.str();
    }

    // Group contexts by PID
    std::map<uint64_t, std::vector<context_health_info::smi_context_health>> contexts_by_pid;
    for (const auto& context : context_health_data) {
      contexts_by_pid[context.pid].push_back(context);
    }

    for (const auto& [pid, contexts] : contexts_by_pid) {
      ss << "  NPU3 Context Health Information (PID: " << pid << "):\n";

      // NPU3-specific table headers - AIE4 microcontroller data
      const std::vector<Table2D::HeaderData> table_headers = {
        {"Ctx.uC",               Table2D::Justification::left},   // Context ID + uC index
        {"FW State",             Table2D::Justification::left},   // Firmware state
        {"uC PC",                Table2D::Justification::left},   // Microcontroller program counter
        {"Exception Addr",       Table2D::Justification::left},   // uC exception address register
        {"Exception Status",     Table2D::Justification::left},   // uC exception status register  
        {"Page.Offset",          Table2D::Justification::left},   // Current page index and offset
        {"Ctx State",            Table2D::Justification::left}    // Context state
      };
      Table2D context_table(table_headers);

      // Add data rows for this PID - NPU3 specific fields using AIE4 structure
      for (const auto& context : contexts) {
        // NPU3 uses AIE4 structure which has per-microcontroller data
        const auto* health = reinterpret_cast<const ert_ctx_health_data_v1*>(context.health_data_raw.data());
        const auto& aie4_data = health->aie4;
        
        if (aie4_data.num_uc == 0) {
          // No microcontroller data available
          const std::vector<std::string> entry_data = {
            (boost::format("%d") % context.ctx_id).str(),
            "No uC data",
            "N/A",
            "N/A", 
            "N/A",
            "N/A",
            (boost::format("0x%x") % aie4_data.ctx_state).str()
          };
          context_table.addEntry(entry_data);
        } else {
          // Display data for each microcontroller
          for (uint32_t i = 0; i < aie4_data.num_uc; ++i) {
            const auto& uc = aie4_data.uc_info[i];
            const std::vector<std::string> entry_data = {
              (boost::format("%d.%d") % context.ctx_id % uc.uc_idx).str(),  // Context.uC format
              (boost::format("0x%x") % uc.fw_state).str(),                  // FW state as status
              (boost::format("0x%x") % uc.uc_pc).str(),                     // uC PC
              (boost::format("0x%x") % uc.uc_ear).str(),                    // Exception address as SP
              (boost::format("0x%x") % uc.uc_esr).str(),                    // Exception status as error code
              (boost::format("%d.%d") % uc.page_idx % uc.offset).str(),     // Page.offset as module ID
              (boost::format("0x%x") % aie4_data.ctx_state).str()           // Context state as cycle count
            };
            context_table.addEntry(entry_data);
          }
        }
      }

      ss << context_table.toString("    ");
      ss << "\n";
    } 
  }
  catch (const std::exception& e) {
    ss << "Error retrieving NPU3 context health data: " << e.what() << "\n";
  }

  return ss.str();
}

void
ReportContextHealth::
writeReport(const xrt_core::device* device,
            const bpt& /*pt*/,
            const std::vector<std::string>& elements_filter,
            std::ostream& output) const
{
  // Detect hardware type and create appropriate reporter
  const auto& pcie_id = xrt_core::device_query<xrt_core::query::pcie_id>(device);
  xrt_core::smi::smi_hardware_config smi_hrdw;
  auto hardware_type = smi_hrdw.get_hardware_type(pcie_id);

  auto reporter = create_reporter(hardware_type);

  // Parse context_id/pid pairs from elements_filter
  std::vector<std::pair<uint64_t, uint64_t>> context_pid_pairs = parse_context_pid_pairs(elements_filter);
  std::vector<uint64_t> context_ids = parse_context_ids(elements_filter);

  // Check for watch mode
  if (smi_watch_mode::parse_watch_mode_options(elements_filter)) {
    // Create report generator lambda for watch mode
    auto report_generator = [&](const xrt_core::device* dev) -> std::string {
      return reporter->generate_report(dev, context_pid_pairs, context_ids);
    };
    
    smi_watch_mode::run_watch_mode(device, output, report_generator);
    return;
  }
  
  // Non-watch mode: generate and output the report
  output << reporter->generate_report(device, context_pid_pairs, context_ids);
  output << std::endl;
}

std::string
ReportContextHealth::
generate_report(const xrt_core::device* /*dev*/,
                const std::vector<std::pair<uint64_t, uint64_t>>& /*context_pid_pairs*/,
                const std::vector<uint64_t>& /*context_ids*/) const
{
  return "Base ReportContextHealth generate_report called - should be overridden in derived class.\n";
}
