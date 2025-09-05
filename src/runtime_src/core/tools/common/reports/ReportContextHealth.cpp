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
#include <sstream>
#include <vector>

using bpt = boost::property_tree::ptree;
using context_health_info = xrt_core::query::context_health_info;

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
    auto context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev);

    // Convert structured data to property tree
    bpt contexts_array{};
    for (const auto& context : context_health_data) {
      bpt context_pt;
      context_pt.put("ctx_id"                     ,context.ctx_id);
      context_pt.put("txn_op_idx"                 ,context.health_data.txn_op_idx);
      context_pt.put("ctx_pc"                     ,context.health_data.ctx_pc);
      context_pt.put("fatal_error_type"           ,context.health_data.fatal_error_type);
      context_pt.put("fatal_error_exception_type" ,context.health_data.fatal_error_exception_type);
      context_pt.put("fatal_error_exception_pc"   ,context.health_data.fatal_error_exception_pc);
      context_pt.put("fatal_error_app_module"     ,context.health_data.fatal_error_app_module);
      contexts_array.push_back(std::make_pair(""  ,context_pt));
    }
    context_health_pt.add_child("contexts", contexts_array);
    context_health_pt.put("context_count", context_health_data.size());
  } 
  catch (const std::exception& e) {
    context_health_pt.put("context_count", 0);
    context_health_pt.put("error", e.what());
  }

  // There can only be 1 root node
  pt.add_child("context_health", context_health_pt);
}

static std::vector<uint32_t> 
parse_values(const std::string& input) 
{
  std::vector<uint32_t> result;
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
        uint32_t value = std::stoul(token);
        result.push_back(value);
      } 
      catch (const std::exception&) {
        // Skip invalid entries
      }
    }
  }
  
  return result;
}

// Helper function to parse context IDs and PIDs and create pairs
// Usage: ctx_id=1,2,3 pid=100,200,300 creates pairs (1,100), (2,200), (3,300)
static std::vector<std::pair<uint32_t, uint32_t>> 
parse_context_pid_pairs(const std::vector<std::string>& elements_filter) 
{
  std::vector<uint32_t> context_ids;
  std::vector<uint32_t> pids;
  
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
  std::vector<std::pair<uint32_t, uint32_t>> pairs;
  size_t max_size = std::max(context_ids.size(), pids.size());
  
  for (size_t i = 0; i < max_size; ++i) {
    uint32_t ctx_id = (i < context_ids.size()) ? context_ids[i] : 0;
    uint32_t pid = (i < pids.size()) ? pids[i] : 0;
    pairs.emplace_back(ctx_id, pid);
  }
  
  return pairs;
}

static std::vector<uint32_t> 
parse_context_ids(const std::vector<std::string>& elements_filter) 
{
  for (const auto& element : elements_filter) {
    if (element.find("ctx_id=") == 0) {
      std::string ctx_ids_str = element.substr(7); // Skip "ctx_id="
      return parse_values(ctx_ids_str);
    }
  }
  return {};
}

static std::string
generate_context_health_report(const xrt_core::device* dev,
                               const std::vector<std::pair<uint32_t, uint32_t>>& context_pid_pairs,
                               const std::vector<uint32_t>& context_ids)
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

    // Create Table2D with headers
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

    // Add data rows
    for (const auto& context : context_health_data) {
      const std::vector<std::string> entry_data = {
        (boost::format("%d")   % context.ctx_id).str(),
        (boost::format("0x%x") % context.health_data.txn_op_idx).str(),
        (boost::format("0x%x") % context.health_data.ctx_pc).str(),
        (boost::format("0x%x") % context.health_data.fatal_error_type).str(),
        (boost::format("0x%x") % context.health_data.fatal_error_exception_type).str(),
        (boost::format("0x%x") % context.health_data.fatal_error_exception_pc).str(),
        (boost::format("0x%x") % context.health_data.fatal_error_app_module).str()
      };
      context_table.addEntry(entry_data);
    }

    ss << "  Context Health Information:\n";
    ss << context_table.toString("    ");

  } 
  catch (const std::exception& e) {
    ss << "Error retrieving context health data: " << e.what() << "\n";
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
  // Parse context_id/pid pairs from elements_filter
  std::vector<std::pair<uint32_t, uint32_t>> context_pid_pairs = parse_context_pid_pairs(elements_filter);
  std::vector<uint32_t> context_ids = parse_context_ids(elements_filter);

  // Check for watch mode
  if (smi_watch_mode::parse_watch_mode_options(elements_filter)) {
    // Create report generator lambda for watch mode
    auto report_generator = [&](const xrt_core::device* dev) -> std::string {
      return generate_context_health_report(dev, context_pid_pairs, context_ids);
    };
    
    smi_watch_mode::run_watch_mode(device, output, report_generator, "Context Health");
    return;
  }
  
  // Non-watch mode: use the same API but without timestamp
  output << "Context Health Report\n";
  output << "=====================\n\n";
  output << generate_context_health_report(device, context_pid_pairs, context_ids);
  output << std::endl;
}
