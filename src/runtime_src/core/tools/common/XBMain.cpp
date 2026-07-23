// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "core/common/system.h"
#include "core/common/sysinfo.h"
#include "core/common/query_requests.h"
#include "SmiDefault.h"
#include "SubCmd.h"
#include "XBHelpMenusCore.h"
#include "XBUtilitiesCore.h"
#include "XBHelpMenus.h"
#include "XBMain.h"
#include "XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <sstream>

namespace po = boost::program_options;

// System - Include Files
#include <filesystem>
#include <iostream>

// ------ Program entry point -------------------------------------------------
void  main_(int argc, char** argv, 
            const std::string & _executable,
            const std::string & _description,
            const SubCmdsCollection &_subCmds) 
{
  bool isUserDomain = boost::iequals(_executable, "xrt-smi"); 

  // Global options
  bool bVerbose = false;
  bool bTrace = false;
  bool bHelp = false;
  bool bBatchMode = false;
  bool bShowHidden = false;
  bool bAdvance = false;
  bool bForce = false;
  bool bVersion = false;
  std::string sDevice;
  std::string sCmd;

  // Build Options
  po::options_description globalSubCmdOptions("Global Command Options");
  globalSubCmdOptions.add_options()
    ("verbose", boost::program_options::bool_switch(&bVerbose), "Turn on verbosity")
    ("batch",   boost::program_options::bool_switch(&bBatchMode), "Enable batch mode (disables escape characters)")
    ("force",   boost::program_options::bool_switch(&bForce), "When possible, force an operation")
  ;

  po::options_description globalOptions("Global Options");
  globalOptions.add_options()
    ("help",    boost::program_options::bool_switch(&bHelp), "Help to use this application")
    ("version", boost::program_options::bool_switch(&bVersion), "Report the version of XRT and its drivers")
  ;
  globalOptions.add(globalSubCmdOptions);

  // Hidden Options
  const std::string device_default = xrt_core::get_total_devices(isUserDomain).first == 1 ? "default" : "";
  po::options_description hiddenOptions("Hidden Options");
  hiddenOptions.add_options()
    ("device,d",    boost::program_options::value<decltype(sDevice)>(&sDevice)->default_value(device_default)->implicit_value("default"), "Specify a BDF. If the option is omitted and there is only 1 device, that device is used\n")
    ("trace",       boost::program_options::bool_switch(&bTrace), "Enables code flow tracing")
    ("show-hidden", boost::program_options::bool_switch(&bShowHidden), "Shows hidden options and commands")
    ("subCmd",      po::value<decltype(sCmd)>(&sCmd), "Command to execute")
  ;

  if (xrt_core::sysinfo::is_advanced()) {
    globalOptions.add_options()
      ("advanced", boost::program_options::bool_switch(&bAdvance), "Shows advanced options and commands")
    ;
  }

  // Merge the options to one common collection
  po::options_description allOptions("All Options");
  allOptions.add(globalOptions).add(hiddenOptions);

  // Create a sub-option command and arguments
  po::positional_options_description positionalCommand;
  positionalCommand.add("subCmd", 1 /* max_count */);

  SubCmdsCollection parsedSubCmds = _subCmds;
  po::variables_map vm;
  po::command_line_parser parser(argc, argv);
  SubCmd::SubCmdOptions subcmd_options;
  try {
    subcmd_options = XBU::process_arguments(vm, parser, allOptions, positionalCommand, false);
    XBU::resolve_device(isUserDomain, vm, sDevice);
    const auto filtered = XBU::filter_subcmds(isUserDomain, sDevice, _subCmds);
    if (!filtered.empty())
      parsedSubCmds = filtered;
    if (sCmd.empty() && !subcmd_options.empty())
    {
      std::string error_str;
      error_str.append("Unrecognized arguments:\n");
      for (const auto& option : subcmd_options)
        error_str.append(boost::str(boost::format("  %s\n") % option));
      throw boost::program_options::error(error_str);
    }
  } catch (po::error& ex) {
    std::cerr << ex.what() << std::endl;
    XBU::report_commands_help( _executable, _description, globalOptions, hiddenOptions, parsedSubCmds);
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if(bVersion) {
    std::cout << XBU::get_xrt_pretty_version();
    return;
  }

  // Check that the versions of XRT for build and tool match
  XBU::xrt_version_cmp(isUserDomain);
 
  // -- Enable/Disable helper "global" options
  XBU::disable_escape_codes( bBatchMode );
  XBU::setVerbose( bVerbose );
  XBU::setTrace( bTrace );
  XBU::setShowHidden( bShowHidden );
  XBU::setAdvance( bAdvance );
  XBU::setForce( bForce );

  // Search for the subcommand (case sensitive)
  std::shared_ptr<SubCmd> subCommand;
  for (auto & subCmdEntry : parsedSubCmds) {
    if (sCmd.compare(subCmdEntry->getName()) == 0) {
      subCommand = subCmdEntry;
      break;
    }
  }

  if (!subCommand) {
    if (!bHelp && !sCmd.empty())
      std::cerr << "ERROR: " << "Unknown command: '" << sCmd << "'" << std::endl;
    XBU::report_commands_help( _executable, _description, globalOptions, hiddenOptions, parsedSubCmds);
    if (!bHelp && !sCmd.empty())
      throw xrt_core::error(std::errc::operation_canceled);
    return;
  }

  // -- Prepare the data
  subcmd_options.erase(subcmd_options.begin());

  if (bHelp)
    subcmd_options.push_back("--help");

  // If there is a device value, pass it to the sub commands.
  if (!sDevice.empty()) {
    subcmd_options.push_back("-d");
    subcmd_options.push_back(sDevice);
  }

  subCommand->setGlobalOptions(globalSubCmdOptions);

  if (isUserDomain){
    /* xrt-smi. Tool should query device upfront and get the configurations
    * from shim. This moves the resposibility for option setting to each shim
    * instead of xrt-smi. 
    * If the device is not found, then load the default xrt-smi config.
    */
    boost::property_tree::ptree configTreeMain;
    std::string config;

    boost::property_tree::ptree available_devices = XBU::get_available_bdfs(isUserDomain);

    if (available_devices.empty()) //no device
      config = xrt_smi_default::get_default_smi_config();
    else if (available_devices.size() == 1 || !sDevice.empty()) { //1 device
      auto device = XBU::get_device(boost::algorithm::to_lower_copy(sDevice), isUserDomain);
      config = xrt_core::device_query<xrt_core::query::xrt_smi_config>(device, xrt_core::query::xrt_smi_config::type::options_config);
    }
    else { //multiple devices
      std::string dev;
      for (auto& kd : available_devices) {
        boost::property_tree::ptree& devpt = kd.second;
        dev = devpt.get<std::string>("bdf");
      }
      std::cout <<  (boost::format("NOTE: Multiple devices found. Showing help for %s device\n\n") % dev).str();
      auto device = XBU::get_device(boost::algorithm::to_lower_copy(dev), isUserDomain);
      config = xrt_core::device_query<xrt_core::query::xrt_smi_config>(device, xrt_core::query::xrt_smi_config::type::options_config);
    }

    std::istringstream command_config_stream(config);
    boost::property_tree::read_json(command_config_stream, configTreeMain);
    subCommand->setOptionConfig(configTreeMain);

    if (XBU::getAdvance())
      XBU::printAdvancedDisclaimer();
  }

  // -- Execute the sub-command
  subCommand->execute(subcmd_options);
}
