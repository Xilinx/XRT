// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "tools/common/SubCmd.h"
#include "tools/common/XBHelpMenusCore.h"
#include "XBFMain.h"
#include "tools/common/XBUtilitiesCore.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <cstdlib>

namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ------ Program entry point -------------------------------------------------
void  main_(int argc, char** argv, 
            const std::string & _executable,
            const std::string & _description,
            const SubCmdsCollection &_subCmds) 
{
  // Global options
  bool bVerbose = false;
  bool bTrace = false;
  bool bHelp = false;
  bool bBatchMode = false;
  bool bAdvance = false;
  bool bForce = false;
  std::string sDevice;

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
  ;
  globalOptions.add(globalSubCmdOptions);

  // Hidden Options
  po::options_description hiddenOptions("Hidden Options");
  hiddenOptions.add_options()
    ("device,d",    boost::program_options::value<decltype(sDevice)>(&sDevice)->default_value("")->implicit_value("default"), "If specified with no BDF value and there is only 1 device, that device will be automatically selected.\n")
    ("trace",       boost::program_options::bool_switch(&bTrace), "Enables code flow tracing")
    ("advance", boost::program_options::bool_switch(&bAdvance), "Shows hidden options and commands")
    ("subCmd",      po::value<std::string>(), "Command to execute")
    ("subCmdArgs",  po::value<std::vector<std::string> >(), "Arguments for command")
  ;

  // Merge the options to one common collection
  po::options_description allOptions("All Options");
  allOptions.add(globalOptions).add(hiddenOptions);

  // Create a sub-option command and arguments
  po::positional_options_description positionalCommand;
  positionalCommand.
    add("subCmd", 1 /* max_count */).
    add("subCmdArgs", -1 /* Unlimited max_count */);

  // -- Parse the command line
  po::parsed_options parsed = po::command_line_parser(argc, argv).
    options(allOptions).            // Global options
    positional(positionalCommand).  // Our commands
    allow_unregistered().           // Allow for unregistered options (needed for sub options)
    run();                          // Parse the options

  po::variables_map vm;

  try {
    po::store(parsed, vm);          // Can throw
    po::notify(vm);                 // Can throw
  } catch (po::error& e) {
    // Something bad happen with parsing our options
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    XBU::report_commands_help(_executable, _description, globalOptions, hiddenOptions, _subCmds);
    throw std::errc::operation_canceled;
  }

  // -- Enable/Disable helper "global" options
  XBU::disable_escape_codes( bBatchMode );
  XBU::setVerbose( bVerbose );
  XBU::setTrace( bTrace );
  XBU::setAdvance( bAdvance );
  XBU::setForce( bForce );

  // Check to see if help was requested and no command was found
  if (vm.count("subCmd") == 0) {
    XBU::report_commands_help( _executable, _description, globalOptions, hiddenOptions, _subCmds);
    return;
  }

  // -- Now see if there is a command to work with
  // Get the command of choice
  std::string sCommand = vm["subCmd"].as<std::string>();

  // Search for the subcommand (case sensitive)
  std::shared_ptr<SubCmd> subCommand;
  for (auto & subCmdEntry :  _subCmds) {
    if (sCommand.compare(subCmdEntry->getName()) == 0) {
      subCommand = subCmdEntry;
      break;
    }
  }

  if ( !subCommand) {
    std::cerr << "ERROR: " << "Unknown command: '" << sCommand << "'" << std::endl;
    XBU::report_commands_help( _executable, _description, globalOptions, hiddenOptions, _subCmds);
    throw std::errc::operation_canceled;
  }

  // -- Prepare the data
  std::vector<std::string> opts = po::collect_unrecognized(parsed.options, po::include_positional);
  opts.erase(opts.begin());

  if (bHelp == true) 
    opts.push_back("--help");

  #ifdef ENABLE_DEFAULT_ONE_DEVICE_OPTION
  // If the user has NOT specified a device AND the command to be executed
  // is not the examine command, then automatically add the device.
  // Note: "examine" produces different reports depending if the user has
  //       specified the --device option or not.
  if ( sDevice.empty() &&
       (subCommand->isDefaultDeviceValid())) {
    sDevice = "default";
  }
  #endif


  // If there is a device value, pass it to the sub commands.
  if (!sDevice.empty()) {
    opts.push_back("-d");
    opts.push_back(sDevice);
  }

  subCommand->setGlobalOptions(globalSubCmdOptions);

  // -- Execute the sub-command
  subCommand->execute(opts);
}



