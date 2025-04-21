// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_Preemption.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenusCore.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Preemption::OO_Preemption( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Force enable|disable and see status of preemption")
    , m_device("")
    , m_action("")
    , m_type("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
    ("type,t", boost::program_options::value<decltype(m_type)>(&m_type), "The type of force-preemption to toggle:\n"
                                    "  layer         - Layer boundary force preeemption\n"
                                    "  frame         - Frame boundary force preeemption\n")
  ;

  m_optionsHidden.add_options()
    ("action", boost::program_options::value<decltype(m_action)>(&m_action), "Action to perform: enable, disable, status");
  ;

  m_positionalOptions.
    add("action", 1 /* max_count */)
  ;
}

void
OO_Preemption::validate_args() const {
  if(m_action.empty())
    throw xrt_core::error(std::errc::operation_canceled, "Please specify a action 'enable', 'disable' or 'status'");
  std::vector<std::string> vec_action { "enable", "disable", "status" };
  if (std::find(vec_action.begin(), vec_action.end(), m_action) == vec_action.end()) {
    throw xrt_core::error(std::errc::operation_canceled, boost::str(boost::format("\n'%s' is not a valid action for force-preemption\n") % m_action));
  }

  if(boost::iequals(m_action, "status"))
    return;

  if(m_type.empty())
    throw xrt_core::error(std::errc::operation_canceled, "Please specify a type using --type");
  std::vector<std::string> vec_type { "layer", "frame" };
  if (std::find(vec_type.begin(), vec_type.end(), m_type) == vec_type.end()) {
    throw xrt_core::error(std::errc::operation_canceled, boost::str(boost::format("\n'%s' is not a valid type of force-preemption\n") % m_type));
  }
}

void
OO_Preemption::execute(const SubCmdOptions& _options) const
{
  XBUtilities::verbose("SubCommand option: Pre-emption");

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
    all_options.add(m_optionsHidden);
    po::command_line_parser parser(_options);
    XBUtilities::process_arguments(vm, parser, all_options, m_positionalOptions, true);

    //validate required arguments
    validate_args(); 
  } catch(boost::program_options::error& ex) {
    std::cout << ex.what() << std::endl;
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  } catch(xrt_core::error& err) {
    std::cout << err.what() << std::endl;
    printHelp();
    throw xrt_core::error(err.get_code());
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

  //show status
  auto int_to_status = [](uint32_t state) -> std::string {
    return state == 0 ? "disabled" : "enabled";
  };

  if(boost::iequals(m_action, "status")) {
    const auto layer_boundary = xrt_core::device_query_default<xrt_core::query::preemption>(device.get(), 0);
    const auto frame_boundary = xrt_core::device_query_default<xrt_core::query::frame_boundary_preemption>(device.get(), 0);
    std::cout << (boost::format("Layer boundary force preemption is %s") % int_to_status(layer_boundary)) << std::endl;
    std::cout << (boost::format("Frame boundary force preemption is %s\n") % int_to_status(frame_boundary)) << std::endl;
    return;
  }

  XBUtilities::sudo_or_throw("Force-preemption requires admin privileges");

  auto action_to_int = [](const std::string& action) -> uint32_t {
    return action == "enable" ? 1 : 0;
  };

  auto pretty_print = [](const std::string& type) -> std::string {
    return (boost::iequals(type, "frame")) ? "Frame boundary" : "Layer boundary";
  };

  try {
    if (boost::iequals(m_type, "frame")) {
      xrt_core::device_update<xrt_core::query::frame_boundary_preemption>(device.get(), action_to_int(m_action));
    }
    else {
      xrt_core::device_update<xrt_core::query::preemption>(device.get(), action_to_int(m_action));
    }
    std::cout << boost::format("\n%s preemption has been %sd \n") % pretty_print(m_type) % (boost::algorithm::to_lower_copy(m_action));
  }
  catch(const xrt_core::error& e) {
    std::cerr << boost::format("\nERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}
