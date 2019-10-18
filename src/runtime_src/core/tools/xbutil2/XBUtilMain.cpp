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
#include "XBUtilMain.h"
#include "SubCmdQuery.h"
#include "SubCmdClock.h"
#include "SubCmdDmaTest.h"
#include "SubCmdDump.h"
#include "SubCmdM2MTest.h"
#include "SubCmdScan.h"
#include "SubCmdProgram.h"
#include "SubCmdRun.h"
#include "SubCmdFan.h"
#include "SubCmdList.h"
#include "SubCmdMem.h"
#include "SubCmdDD.h"
#include "SubCmdTop.h"
#include "SubCmdValidate.h"
#include "SubCmdReset.h"
#include "SubCmdP2P.h"
#include "SubCmdVersion.h"

#include "XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>


static void printHelp()
{
    std::cout << "Running xbutil for 4.0+ shell's \n\n";
    std::cout << "Usage: " << "xbutil" << " <command> [options]\n\n";
    std::cout << "Command and option summary:\n";
    std::cout << "  clock   [-d card] [-r region] [-f clock1_freq_MHz] [-g clock2_freq_MHz] [-h clock3_freq_MHz]\n";
    std::cout << "  dmatest [-d card] [-b [0x]block_size_KB]\n";
    std::cout << "  dump\n";
    std::cout << "  help\n";
    std::cout << "  m2mtest\n";
    std::cout << "  mem --read [-d card] [-a [0x]start_addr] [-i size_bytes] [-o output filename]\n";
    std::cout << "  mem --write [-d card] [-a [0x]start_addr] [-i size_bytes] [-e pattern_byte]\n";
    std::cout << "  program [-d card] [-r region] -p xclbin\n";
    std::cout << "  query   [-d card [-r region]]\n";
    std::cout << "  status [-d card] [--debug_ip_name]\n";
    std::cout << "  scan\n";
    std::cout << "  top [-i seconds]\n";
    std::cout << "  validate [-d card]\n";
    std::cout << " Requires root privileges:\n";
    std::cout << "  reset  [-d card]\n";
    std::cout << "  p2p    [-d card] --enable\n";
    std::cout << "  p2p    [-d card] --disable\n";
    std::cout << "  p2p    [-d card] --validate\n";
    std::cout << "\nExamples:\n";
    std::cout << "Print JSON file to stdout\n";
    std::cout << "  " << "xbutil" << " dump\n";
    std::cout << "List all cards\n";
    std::cout << "  " << "xbutil" << " list\n";
    std::cout << "Scan for Xilinx PCIe card(s) & associated drivers (if any) and relevant system information\n";
    std::cout << "  " << "xbutil" << " scan\n";
    std::cout << "Change the clock frequency of region 0 in card 0 to 100 MHz\n";
    std::cout << "  " << "xbutil" << " clock -f 100\n";
    std::cout << "For card 0 which supports multiple clocks, change the clock 1 to 200MHz and clock 2 to 250MHz\n";
    std::cout << "  " << "xbutil" << " clock -f 200 -g 250\n";
    std::cout << "Download the accelerator program for card 2\n";
    std::cout << "  " << "xbutil" << " program -d 2 -p a.xclbin\n";
    std::cout << "Run DMA test on card 1 with 32 KB blocks of buffer\n";
    std::cout << "  " << "xbutil" << " dmatest -d 1 -b 0x2000\n";
    std::cout << "Read 256 bytes from DDR starting at 0x1000 into file read.out\n";
    std::cout << "  " << "xbutil" << " mem --read -a 0x1000 -i 256 -o read.out\n";
    std::cout << "  " << "Default values for address is 0x0, size is DDR size and file is memread.out\n";
    std::cout << "Write 256 bytes to DDR starting at 0x1000 with byte 0xaa \n";
    std::cout << "  " << "xbutil" << " mem --write -a 0x1000 -i 256 -e 0xaa\n";
    std::cout << "  " << "Default values for address is 0x0, size is DDR size and pattern is 0x0\n";
    std::cout << "List the debug IPs available on the platform\n";
    std::cout << "  " << "xbutil" << " status \n";
    std::cout << "Validate installation on card 1\n";
    std::cout << "  " << "xbutil" << " validate -d 1\n";
}



// Initialized the sub cmd call back table
typedef int (*t_subcommand)(const std::vector<std::string> &, bool);
static const std::map<const std::string, t_subcommand> cmdTable = {
  {"query",    &subCmdQuery},
  {"program",  &subCmdProgram},
  {"clock",    &subCmdClock},
  {"dump",     &subCmdDump},
  {"help",     nullptr},
  {"run",      &subCmdRun},               // Should this be hidden?
  {"fan",      &subCmdFan},               // Should this be hidden?
  {"dmatest",  &subCmdDmaTest},
  {"list",     &subCmdList},
  {"mem",      &subCmdMem},
  {"dd",       &subCmdDD},                // Should this be hidden?
  {"status",   &subCmdScan},
  {"m2mtest",  &subCmdM2MTest},
  {"top",      &subCmdTop},
  {"validate", &subCmdValidate},
  {"reset",    &subCmdReset},
  {"p2p",      &subCmdP2P},
  {"version",  &subCmdVersion}
};


// ------ Program entry point -------------------------------------------------
ReturnCodes main_(int argc, char** argv) {

  // Global options
  bool bVerbose = false;
  bool bHelp = false;

  // Build our global options
  po::options_description globalOptions("Global options");
  globalOptions.add_options()
    ("help", boost::program_options::bool_switch(&bHelp), "Help to use this program")
    ("verbose", boost::program_options::bool_switch(&bVerbose), "Turn on verbosity")
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

  // Set the verbosity if enabled
  if (bVerbose == true) {
    XBU::setVerbose( true );
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

  if (cmdTable.find(sCommand) == cmdTable.end()) {
    std::cerr << "ERROR: " << "Unknown sub-command: '" << sCommand << "'" << std::endl;
    return RC_ERROR_IN_COMMAND_LINE;
  }

  // Prepare the data
  std::vector<std::string> opts = po::collect_unrecognized(parsed.options, po::include_positional);
  opts.erase(opts.begin());

  // Call the registered function for this command
  if (cmdTable.find(sCommand)->second != nullptr) {
    cmdTable.find(sCommand)->second(opts, bHelp);
  }

  return RC_SUCCESS;
}



