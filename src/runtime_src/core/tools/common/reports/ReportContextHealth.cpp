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
      context_pt.put("txn_op_idx", context.txn_op_idx);
      context_pt.put("ctx_pc", context.ctx_pc);
      context_pt.put("fatal_error_type", context.fatal_error_type);
      context_pt.put("fatal_error_exception_type", context.fatal_error_exception_type);
      context_pt.put("fatal_error_exception_pc", context.fatal_error_exception_pc);
      context_pt.put("fatal_error_app_module", context.fatal_error_app_module);
      contexts_array.push_back(std::make_pair("", context_pt));
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

// Helper function to parse context IDs from elements_filter
static std::vector<uint32_t> 
parse_context_ids(const std::vector<std::string>& elements_filter) 
{
  std::vector<uint32_t> context_ids;
  std::stringstream ctx_stream;
  
  for (const auto& filter : elements_filter) {
    if (filter.find("ctx_id=") == 0) {
      std::string ctx_ids_str = filter.substr(7); // // NOLINT(cppcoreguidelines-avoid-magic-numbers) 
      ctx_stream.str(ctx_ids_str);
      ctx_stream.clear(); // Clear error flags
      std::string ctx_id_token;
      
      // Parse comma-separated context IDs
      while (std::getline(ctx_stream, ctx_id_token, ',')) {
        try {
          uint32_t ctx_id = std::stoul(ctx_id_token);
          context_ids.push_back(ctx_id);
        } 
        catch (const std::exception&) {
          // Invalid ctx_id format, skip this token
        }
      }
      break;
    }
  }
  
  return context_ids;
}

static std::string
generate_context_health_report(const xrt_core::device* dev,
                               const std::vector<std::string>& elements_filter,
                               bool include_timestamp = true)
{
  std::stringstream ss;
  
  // Parse context IDs from elements_filter - collect for future use
  std::vector<uint32_t> context_ids = parse_context_ids(elements_filter);

  try {
    std::vector<ert_ctx_health_data> context_health_data;
    
    // Query device with context_ids parameter if provided
    if (context_ids.empty()) {
      // No specific context IDs - get all contexts
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev);
    } else {
      // Pass context_ids vector to device query
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev, context_ids);
    }
    
    auto context_count = context_health_data.size();
    
    if (include_timestamp) {
      ss << boost::format("Context Health Report (Total: %d) - %s\n") % context_count % xrt_core::timestamp();
      ss << "=======================================================\n\n";
    } else {
      ss << boost::format("Total Contexts: %d\n\n") % context_count;
    }

    if (context_count == 0) {
      ss << "No context health data available\n";
      return ss.str();
    }

    // Create Table2D with headers
    const std::vector<Table2D::HeaderData> table_headers = {
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
        (boost::format("0x%x") % context.txn_op_idx).str(),
        (boost::format("0x%x") % context.ctx_pc).str(),
        std::to_string(context.fatal_error_type),
        std::to_string(context.fatal_error_exception_type),
        (boost::format("0x%x") % context.fatal_error_exception_pc).str(),
        std::to_string(context.fatal_error_app_module)
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
  // Check for watch mode
  if (smi_watch_mode::parse_watch_mode_options(elements_filter)) {
    // Create report generator lambda for watch mode
    auto report_generator = [](const xrt_core::device* dev, const std::vector<std::string>& filters) -> std::string {
      return generate_context_health_report(dev, filters, true); // include timestamp for watch mode
    };
    
    smi_watch_mode::run_watch_mode(device, elements_filter, output, 
                                  report_generator, "Context Health");
    return;
  }
  
  // Non-watch mode: use the same API but without timestamp
  output << "Context Health Report\n";
  output << "=====================\n\n";
  output << generate_context_health_report(device, elements_filter, false); // no timestamp for non-watch mode
  output << std::endl;
}
