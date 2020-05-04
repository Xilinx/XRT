/**
 * Copyright (C) 2019-2020 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "XBMain.h"

#include "XBUtilities.h"
#include "SubCmd.h"
#include "XBHelpMenus.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

// System - Include Files
#include <iostream>

// ------ Program entry point -------------------------------------------------
void  main_(int argc, char** argv, 
            const std::string & _description,
            const SubCmdsCollection &_subCmds) 
{
  if (_subCmds.size() == 0) {
    // do nothing
  }
  // Determine the executable name for this application
  boost::filesystem::path pathAndFile(argv[0]);
  std::string executable = pathAndFile.filename().string();
  executable.pop_back();

  // Global options
  bool bVerbose = false;
  bool bTrace = false;
  bool bHelp = false;
  bool bBatchMode = false;

  // Build our options
  po::options_description globalOptions("Global Options");
  globalOptions.add_options()
    ("help,h", boost::program_options::bool_switch(&bHelp), "Help to use this application")
    ("verbose,v", boost::program_options::bool_switch(&bVerbose), "Turn on verbosity")
    ("batch,b", boost::program_options::bool_switch(&bBatchMode), "Enable batch mode (disables escape characters)")

  ;

  po::options_description hiddenOptions("Hidden Options");
  hiddenOptions.add_options()
    ("trace", boost::program_options::bool_switch(&bTrace), "Enables code flow tracing")
    ("subCmd", po::value<std::string>(), "command to execute")
    ("subCmdArgs", po::value<std::vector<std::string> >(), "Arguments for command")
  ;
  // Merge the options to one common collection
  po::options_description allOptions("Allowed Options");
  allOptions.add(globalOptions).add(hiddenOptions);

  // Create a sub-option command and arguments
  po::positional_options_description positionalCommand;
  positionalCommand.
    add("subCmd", 1 /* max_count */).
    add("subCmdArgs", -1 /* Unlimited max_count */);

  // Parse the command line
  po::parsed_options parsed = po::command_line_parser(argc, argv).
    options(allOptions).            // Global options
    positional(positionalCommand).  // Our commands
    allow_unregistered().           // Allow for unregistered options (needed for sub obtions)
    run();                          // Parse the options

  po::variables_map vm;

  try {
    po::store(parsed, vm);          // Can throw
    po::notify(vm);                 // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    XBU::report_commands_help(executable, _description, globalOptions, _subCmds);
    return;
  }
 
  // Disable escape characters if in batch mode
  XBU::disable_escape_codes( bBatchMode );

  // Set the verbosity if enabled
  if (bVerbose == true) 
    XBU::setVerbose( true );

  // Set the tracing if enabled
  if (bTrace == true) {
    XBU::setTrace( true );
  }

  // Check to see if help was requested and no command was found
  if (vm.count("subCmd") == 0) {
    XBU::report_commands_help(executable, _description, globalOptions, _subCmds);
    return;
  }

  // Now see if there is a command to work with
  // Get the command of choice
  std::string sCommand = vm["subCmd"].as<std::string>();

  if (sCommand == "help") {
    XBU::report_commands_help(executable, _description, globalOptions, _subCmds);
    return;
  }

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
    XBU::report_commands_help(executable, _description, globalOptions, _subCmds);
    return;
  }

  // Prepare the data
  std::vector<std::string> opts = po::collect_unrecognized(parsed.options, po::include_positional);
  opts.erase(opts.begin());

  if (bHelp == true) {
      opts.push_back("--help");
  }

  // Execute the sub-command
  subCommand->execute(opts);

  return;
}



