/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include "XBMgmtMain.h"

#include "XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>


static void printHelp()
{
    std::cout << "<------TO-DO------->\n";
    std::cout << "\tPrint help\n";
}



// Initialized the sub cmd call back table
// typedef int (*t_subcommand)(const std::vector<std::string> &, bool);
// static const std::map<const std::string, t_subcommand> cmdTable = {
// };


// ------ Program entry point -------------------------------------------------
ReturnCodes main_(int argc, char** argv) {

  // Global options
  bool bVerbose = false;
  bool bTrace = false;
  bool bHelp = false;
  bool bOverride = false;

  // Build our global options
  po::options_description globalOptions("Global options");
  globalOptions.add_options()
    ("help", boost::program_options::bool_switch(&bHelp), "Help to use this program")
    ("verbose", boost::program_options::bool_switch(&bVerbose), "Turn on verbosity")
    ("trace", boost::program_options::bool_switch(&bTrace), "Enables code flow tracing")
    ("override", boost::program_options::bool_switch(&bOverride), "Bypasses the development check for this executable")
    ("command", po::value<std::string>(), "command to execute")
    ("subArguments", po::value<std::vector<std::string> >(), "Arguments for command")
  ;

  // Create a sub-option command and arguments
  po::positional_options_description positionalCommand;
  positionalCommand.
    add("command", 1 /* max_count */).
    add("subArguments", -1 /* Unlimited max_count */);

  // Parse the command line
  po::parsed_options parsed = po::command_line_parser(argc, argv).
    options(globalOptions).         // Global options
    positional(positionalCommand).  // Our commands
    allow_unregistered().           // Allow for unregistered options (needed for sub obtions)
    run();                          // Parse the options

  po::variables_map vm;

  try {
    po::store(parsed, vm);          // Can throw
    po::notify(vm);                 // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << globalOptions << std::endl;
    return RC_ERROR_IN_COMMAND_LINE;
  }

  if (bOverride == false) {
    std::cout << std::endl;
    std::cout << "===========================================================" << std::endl;
    std::cout << "xbutil2 is currently under development and currently is    " << std::endl;
    std::cout << "not ready to be used in examining XRT drivers or platforms." << std::endl;
    std::cout << "===========================================================" << std::endl;
    exit(0);
  }

  // Set the verbosity if enabled
  if (bVerbose == true) {
    XBU::setVerbose( true );
  }

  // Set the tracing if enabled
  if (bTrace == true) {
    XBU::setTrace( true );
  }

  // Check to see if help was requested or no command was found
  if ((bHelp == true) || (vm.count("command") == 0)) {
    ::printHelp();
    return RC_SUCCESS;
  }

  // Now see if there is a command to work with
  // Get the command of choice
  std::string sCommand = vm["command"].as<std::string>();

  if (sCommand == "help") {
    ::printHelp();
    return RC_SUCCESS;
  }

//   if (cmdTable.find(sCommand) == cmdTable.end()) {
//     std::cerr << "ERROR: " << "Unknown sub-command: '" << sCommand << "'" << std::endl;
//     return RC_ERROR_IN_COMMAND_LINE;
//   }

//   // Prepare the data
//   std::vector<std::string> opts = po::collect_unrecognized(parsed.options, po::include_positional);
//   opts.erase(opts.begin());

//   // Call the registered function for this command
//   if (cmdTable.find(sCommand)->second != nullptr) {
//     cmdTable.find(sCommand)->second(opts, bHelp);
//   }

  return RC_SUCCESS;
}



