// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_ContextHealth.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/SmiWatchMode.h"
#include "tools/common/Table2D.h"
#include "core/common/query_requests.h"
#include "core/common/smi.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <algorithm>
#include <sstream>
#include <vector>
#include <map>
#include <memory>

namespace po = boost::program_options;
using context_health_info = xrt_core::query::context_health_info;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_ContextHealth::OO_ContextHealth( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Display context health information")
    , m_device("")
    , m_help(false)
    , m_watch(false)
    , m_ctx_id_list("")
    , m_pid_list("")
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("watch", boost::program_options::bool_switch(&m_watch), "Continuously monitor context health")
    ("ctx-id", boost::program_options::value<decltype(m_ctx_id_list)>(&m_ctx_id_list), "Comma-separated list of context IDs to filter (e.g., 1,2,3)")
    ("pid", boost::program_options::value<decltype(m_pid_list)>(&m_pid_list), "Comma-separated list of PIDs to filter (e.g., 1234,5678)")
  ;
}

namespace {

// Parse comma-separated values
std::vector<uint64_t> 
parse_values(const std::string& input) 
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

// Parse context ID and PID pairs
std::vector<std::pair<uint64_t, uint64_t>>
parse_context_pid_pairs(const std::string& ctx_id_list, const std::string& pid_list) 
{
  std::vector<uint64_t> context_ids = parse_values(ctx_id_list);
  std::vector<uint64_t> pids = parse_values(pid_list);
  
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

// Generate report for STRX hardware
std::string
generate_strx_report(const xrt_core::device* dev,
                     const std::vector<std::pair<uint64_t, uint64_t>>& context_pid_pairs,
                     const std::vector<uint64_t>& context_ids)
{
  std::stringstream ss;

  try {
    std::vector<context_health_info::smi_context_health> context_health_data;

    // If any pid is nonzero, pass pairs
    bool has_nonzero_pid = std::any_of(context_pid_pairs.begin(), context_pid_pairs.end(), [](const auto& p){ return p.second != 0; });
    if (!context_pid_pairs.empty() && has_nonzero_pid) 
    {
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev, context_pid_pairs);
    } else if (!context_ids.empty()) 
    {
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev, context_ids);
    } else 
    {
      context_health_data = xrt_core::device_query<xrt_core::query::context_health_info>(dev);
    }

    auto context_count = context_health_data.size();

    if (context_count == 0) 
      return ss.str();

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

// Generate report for NPU3 hardware
std::string
generate_npu3_report(const xrt_core::device* dev,
                     const std::vector<std::pair<uint64_t, uint64_t>>& context_pid_pairs,
                     const std::vector<uint64_t>& context_ids)
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

} // anonymous namespace

void
OO_ContextHealth::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Context Health");

  XBUtilities::verbose("Option(s):");
  for (auto & aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::options_description all_options("All Options");
    all_options.add(m_optionsDescription);
    all_options.add(m_optionsHidden);
    po::command_line_parser parser(_options);
    XBUtilities::process_arguments(vm, parser, all_options, m_positionalOptions, true);
  } catch(boost::program_options::error& ex) {
    std::cout << ex.what() << std::endl;
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  } 

  if (m_help)
  {
    printHelp();
    return;
  }

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  
  try {
    device = XBUtilities::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Detect hardware type
  const auto& pcie_id = xrt_core::device_query<xrt_core::query::pcie_id>(device.get());
  xrt_core::smi::smi_hardware_config smi_hrdw;
  auto hardware_type = smi_hrdw.get_hardware_type(pcie_id);

  // Parse filter options
  auto context_ids = parse_values(m_ctx_id_list);
  auto context_pid_pairs = parse_context_pid_pairs(m_ctx_id_list, m_pid_list);

  // Create report generator based on hardware type
  auto report_generator = [&](const xrt_core::device* dev) -> std::string {
    if (XBUtilities::is_strix_hardware(hardware_type))
      return generate_strx_report(dev, context_pid_pairs, context_ids);
    else
      return generate_npu3_report(dev, context_pid_pairs, context_ids);
  };

  if (m_watch) {
    // Watch mode: continuously monitor
    smi_watch_mode::run_watch_mode(device.get(), std::cout, report_generator);
  } else {
    // Single report
    std::cout << report_generator(device.get());
  }
}

