// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportContextHealth.h"
#include "core/common/query_requests.h"
#include "core/common/time.h"
#include "tools/common/ReportWatchMode.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>

using bpt = boost::property_tree::ptree;

// Table layout configuration for context health report
struct TableColumn {
  std::string header;
  int width;
};

static const std::vector<TableColumn>& getContextHealthColumns() {
  static const std::vector<TableColumn> context_health_columns = {
    {"Context ID",           12},
    {"Txn Op Idx",           12},
    {"Ctx PC",               12},
    {"Fatal Err Type",       16},
    {"Fatal Err Ex Type",    18},
    {"Fatal Err Ex PC",      16},
    {"Fatal App Module",     16}
  };
  return context_health_columns;
}

static void generate_table_header(std::stringstream& ss) {
  ss << "  Context Health Information:\n";
  
  const auto& columns = getContextHealthColumns();
  
  // Generate single header row
  ss << "    |";
  for (const auto& col : columns) {
    ss << std::left << std::setw(col.width - 1) << col.header << "|";
  }
  ss << "\n";
  
  // Generate separator line
  ss << "    |";
  for (const auto& col : columns) {
    ss << std::string(col.width - 1, '=') << "|";
  }
  ss << "\n";
}

// Helper structure to hold context data for table generation
struct ContextData {
  uint32_t context_id;
  uint32_t txn_op_idx;
  uint32_t ctx_pc;
  uint32_t fatal_error_type;
  uint32_t fatal_error_exception_type;
  uint32_t fatal_error_exception_pc;
  uint32_t fatal_error_app_module;
};

static void generate_context_data_row(std::stringstream& ss, const ContextData& data) {
  const auto& columns = getContextHealthColumns();
  
  // Single row with all data
  ss << "    |";
  ss << std::left << std::setw(columns[0].width - 1) << data.context_id << "|";
  ss << std::left << std::setw(columns[1].width - 1) << data.txn_op_idx << "|";
  ss << std::left << std::setw(columns[2].width - 1) << (boost::format("0x%x") % data.ctx_pc).str() << "|";
  ss << std::left << std::setw(columns[3].width - 1) << data.fatal_error_type << "|";
  ss << std::left << std::setw(columns[4].width - 1) << data.fatal_error_exception_type << "|";
  ss << std::left << std::setw(columns[5].width - 1) << (boost::format("0x%x") % data.fatal_error_exception_pc).str() << "|"; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
  ss << std::left << std::setw(columns[6].width - 1) << data.fatal_error_app_module << "|\n"; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
  
  // Separator line
  ss << "    |";
  for (const auto& col : columns) {
    ss << std::string(col.width - 1, '-') << "|";
  }
  ss << "\n";
}

void
ReportContextHealth::getPropertyTreeInternal(const xrt_core::device* dev,
                                            bpt& pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data,
  // Then update this method to do so.
  getPropertyTree20202(dev, pt);
}

void
ReportContextHealth::getPropertyTree20202(const xrt_core::device* dev,
                                         bpt& pt) const
{
  bpt context_health_pt{};

  try {
    auto context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev);

    // Convert structured data to property tree
    bpt contexts_array{};
    for (const auto& context : context_health_data) {
      bpt context_pt;
      context_pt.put("context_id", context.context_id);
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
  } catch (const std::exception& e) {
    context_health_pt.put("context_count", 0);
    context_health_pt.put("error", e.what());
  }

  // There can only be 1 root node
  pt.add_child("context_health", context_health_pt);
}

// Helper function to parse context IDs from elements_filter
static std::vector<uint32_t> parse_context_ids(const std::vector<std::string>& elements_filter) {
  std::vector<uint32_t> context_ids;
  
  for (const auto& filter : elements_filter) {
    if (filter.find("ctx_id=") == 0) {
      std::string ctx_ids_str = filter.substr(7); // Extract string after "ctx_id="
      std::stringstream ctx_stream(ctx_ids_str);
      std::string ctx_id_token;
      
      // Parse comma-separated context IDs
      while (std::getline(ctx_stream, ctx_id_token, ',')) {
        try {
          uint32_t ctx_id = std::stoul(ctx_id_token);
          context_ids.push_back(ctx_id);
        } catch (const std::exception&) {
          // Invalid ctx_id format, skip this token
        }
      }
      break;
    }
  }
  
  return context_ids;
}

static std::string
generate_context_health_report(const xrt_core::device* dev, const std::vector<std::string>& elements_filter, bool include_timestamp = true) {
  std::stringstream ss;
  
  // Parse context IDs from elements_filter - collect for future use
  std::vector<uint32_t> context_ids = parse_context_ids(elements_filter);

  try {
    auto context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev);
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

    // Generate table with structured headers
    generate_table_header(ss);
    
    for (const auto& context : context_health_data) {
      // Generate structured data row for all contexts
      ContextData data{
        context.context_id,
        context.txn_op_idx,
        context.ctx_pc,
        context.fatal_error_type,
        context.fatal_error_exception_type,
        context.fatal_error_exception_pc,
        context.fatal_error_app_module
      };
      generate_context_data_row(ss, data);
    }

  } catch (const std::exception& e) {
    ss << "Error retrieving context health data: " << e.what() << "\n";
  }

  return ss.str();
}

void
ReportContextHealth::writeReport(const xrt_core::device* device,
                                const bpt& /*pt*/,
                                const std::vector<std::string>& elements_filter,
                                std::ostream& output) const
{
  // Check for watch mode
  if (report_watch_mode::parse_watch_mode_options(elements_filter)) {
    // Create report generator lambda for watch mode
    auto report_generator = [](const xrt_core::device* dev, const std::vector<std::string>& filters) -> std::string {
      return generate_context_health_report(dev, filters, true); // include timestamp for watch mode
    };
    
    report_watch_mode::run_watch_mode(device, elements_filter, output, 
                                     report_generator, "Context Health");
    return;
  }
  
  // Non-watch mode: use the same API but without timestamp
  output << "Context Health Report\n";
  output << "=====================\n\n";
  output << generate_context_health_report(device, elements_filter, false); // no timestamp for non-watch mode
  output << std::endl;
}
