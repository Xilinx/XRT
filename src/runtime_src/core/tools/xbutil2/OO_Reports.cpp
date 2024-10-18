// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_Reports.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"
#include "core/common/info_platform.h"
#include "core/common/info_telemetry.h"
#include "tools/common/Table2D.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;
using bpt = boost::property_tree::ptree;

namespace {
static void
print_preemption_telemetry(const xrt_core::device* device)
{
  boost::property_tree::ptree empty_ptree;
  std::stringstream ss;
  boost::property_tree::ptree telemetry_pt = xrt_core::telemetry::preemption_telemetry_info(device).get_child("telemetry", empty_ptree);
  ss << "Premption Telemetry Data\n";
  if (telemetry_pt.empty()) {
    ss << " No hardware contexts running on device\n\n";
    std::cout << ss.str();
    return;
  }

  std::vector<Table2D::HeaderData> preempt_headers = {
    {"User Task", Table2D::Justification::left},
    {"Ctx ID", Table2D::Justification::left},
    {"Set Hints", Table2D::Justification::left},
    {"Unset Hints", Table2D::Justification::left},
    {"Checkpoint Events", Table2D::Justification::left},
    {"Frame Boundary Events", Table2D::Justification::left},
  };
  Table2D preemption_table(preempt_headers);

  for (const auto& [name, user_task] : telemetry_pt) {
    const std::vector<std::string> rtos_data = {
      user_task.get<std::string>("user_task"),
      user_task.get<std::string>("slot_index"),
      user_task.get<std::string>("preemption_flag_set"),
      user_task.get<std::string>("preemption_flag_unset"),
      user_task.get<std::string>("preemption_checkpoint_event"),
      user_task.get<std::string>("preemption_frame_boundary_events"),
    };
    preemption_table.addEntry(rtos_data);
  }

  ss << preemption_table.toString("  ") << "\n";

  std::cout << ss.str();
}

} //end namespace;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Reports::OO_Reports( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Hidden reports")
    , m_device("")
    , m_action("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("mode", boost::program_options::value<decltype(m_action)>(&m_action)->required(), "Action to perform: clocks, preemption")
  ;

  m_positionalOptions.
    add("mode", 1 /* max_count */)
  ;
}

void
OO_Reports::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: report");

  XBUtilities::verbose("Option(s):");
  for (auto & aString : _options)
    XBUtilities::verbose(std::string(" ") + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::options_description all_options("All Options");
    all_options.add(m_optionsDescription);
    po::command_line_parser parser(_options);
    XBUtilities::process_arguments(vm, parser, all_options, m_positionalOptions, true);
  } catch(boost::program_options::error&) {
      if(m_help) {
        printHelp();
        throw xrt_core::error(std::errc::operation_canceled);
      }
      // Exit if neither action or device specified
      if(m_action.empty()) {
        std::cerr << boost::format("ERROR: the required argument for option '--report' is missing\n");
        printHelp();
        throw xrt_core::error(std::errc::operation_canceled);
      }
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

  try {
    if (boost::iequals(m_action, "clocks")) {
      bpt empty_tree;
      auto clocks = xrt_core::platform::get_clock_info(device.get());
      const bpt& pt_clock_array = clocks.get_child("clocks", empty_tree);
      if(pt_clock_array.empty())
        return;
    
      std::cout << std::endl << "Clocks" << std::endl;
      for (const auto& kc : pt_clock_array) {
        const bpt& pt_clock = kc.second;
        std::string clock_name_type = pt_clock.get<std::string>("id");
        std::cout << boost::format("  %-23s: %3s MHz\n") % clock_name_type % pt_clock.get<std::string>("freq_mhz");
      }
    }
    else if (boost::iequals(m_action, "preemption")) {
      print_preemption_telemetry(device.get());
    }
    else {
      throw xrt_core::error(boost::str(boost::format("Invalid report value: '%s'\n") % m_action));
    }
  }
  catch(const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
