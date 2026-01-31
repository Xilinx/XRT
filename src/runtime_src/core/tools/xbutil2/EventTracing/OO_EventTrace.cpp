// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_EventTrace.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"
#include "core/common/query_requests.h"
#include "core/common/json/nlohmann/json.hpp"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;
namespace smi = xrt_core::tools::xrt_smi;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_EventTrace::OO_EventTrace( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Enable|disable event trace")
    , m_device("")
    , m_enable(false)
    , m_disable(false)
    , m_help(false)
    , m_list_categories(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("enable", boost::program_options::bool_switch(&m_enable), "Enable event tracing")
    ("disable", boost::program_options::bool_switch(&m_disable), "Disable event tracing")
    ("list-categories", boost::program_options::bool_switch(&m_list_categories), "List available event trace categories")
    ("categories", boost::program_options::value<decltype(m_categories)>(&m_categories)->multitoken(), 
                   "Space-separated list of category names. Use \"all\" to enable all available categories")
  ;
}



uint32_t
OO_EventTrace::
parse_categories(const std::vector<std::string>& categories_list, 
                 const xrt_core::device* device,
                 bool is_enable) const 
{
  // If "all" is specified, enable/disable all categories
  if (categories_list.size() == 1 && categories_list[0] == "all") {
    return 0xFFFFFFFF; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  }

  // If no categories specified with --enable, enable all categories
  if (categories_list.empty() && is_enable) {
    return 0xFFFFFFFF; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  }

  // If no categories specified with --disable, return 0 (no-op or error)
  if (categories_list.empty()) {
    return 0;
  }

  uint32_t category_mask = 0;
  
  // Get category mappings from config
  std::map<std::string, uint32_t> category_map = smi::event_trace_config::get_category_map(device);

  for (const auto& category_name : categories_list) {
    auto it = category_map.find(category_name);
    if (it != category_map.end()) {
      category_mask |= it->second;
    } else {
      // Unknown category, warn the user
      std::cerr << "Warning: Unknown category '" << category_name << "', ignoring\n";
    }
  }
  return category_mask;
}

void
OO_EventTrace::
handle_list_categories(const xrt_core::device* device) const {
  try {
    auto category_map = smi::event_trace_config::get_category_map(device);
    if (!category_map.empty()) {
      std::cout << "Available event trace categories for device " << m_device << ":\n";
      for (const auto& pair : category_map) {
        std::cout << "  " << pair.first << "\n";
      }
    } else {
      std::cout << "No categories available for device " << m_device << "\n";
    }
  } catch (const std::exception& e) {
    std::cerr << "Error loading categories: " << e.what() << "\n";
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

void
OO_EventTrace::
handle_config(const xrt_core::device* device) const {
  uint32_t action_value = m_enable ? 1 : 0;
  std::string action_name = m_enable ? "enable" : "disable";

  try {
    uint32_t category_mask = parse_categories(m_categories, device, m_enable);
    xrt_core::query::event_trace_state::value_type params{action_value, category_mask};
    xrt_core::device_update<xrt_core::query::event_trace_state>(device, params);
    std::cout << "Event trace " << action_name << "d successfully" << std::endl;
  }
  catch(const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

void
OO_EventTrace::
validate_args() const {
  if(!m_enable 
     && !m_disable 
     && !m_help 
     && !m_list_categories)
    throw xrt_core::error(std::errc::operation_canceled, 
          "Please specify an action: --enable, --disable, or --list-categories");
  
  if(m_enable && m_disable)
    throw xrt_core::error(std::errc::operation_canceled, "Cannot specify both --enable and --disable");
}

void
OO_EventTrace::
execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Event Trace");
  XBUtilities::sudo_or_throw("Event tracing requires admin privileges");

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

  if (m_help) {
    printHelp();
    return;
  }

  try {
    //validate required arguments
    validate_args(); 
  } catch(xrt_core::error& err) {
    std::cout << err.what() << std::endl;
    printHelp();
    throw xrt_core::error(err.get_code());
  }

  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBUtilities::get_device(boost::algorithm::to_lower_copy(m_device), true);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if (m_list_categories) {
    handle_list_categories(device.get());
    return;
  }

  if (m_enable || m_disable) {
    handle_config(device.get());
    return;
  }
}
