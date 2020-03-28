/**
 * Copyright (C) 2019-2020
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
#include "SubCmdClock.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdClock::SubCmdClock(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("clock", 
             "See replacement functionality in command: 'advanced'")
{
  const std::string longDescription = "Change a given clock frequecy.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdClock::execute(const SubCmdOptions& _options) const
// Reference Command:  clock   [-d card] [-r region] [-f clock1_freq_MHz] [-g clock2_freq_MHz] [-h clock3_freq_MHz]
//                     Change the clock frequency of region 0 in card 0 to 100 MHz\n";
//                         xbutil clock -f 100
//                     For card 0 which supports multiple clocks, change the clock 1 to 200MHz and clock 2 to 250MHz\n";
//                         xbutil clock -f 200 -g 250;

{
  XBU::verbose("SubCommand: clock");
  // -- Retrieve and parse the subcommand options -----------------------------
  uint64_t card = 0;
  uint64_t region = 0;
  uint64_t clock1FreqMhz = 0;
  uint64_t clock2FreqMhz = 0;
  uint64_t clock3FreqMhz = 0;
  bool help = false;

  po::options_description clockDesc("clock options");
  clockDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<uint64_t>(&card), "Card to be examined")
    (",r", boost::program_options::value<uint64_t>(&region), "Card region")
    (",f", boost::program_options::value<uint64_t>(&clock1FreqMhz), "Clock 1 frequency MHz")
    (",g", boost::program_options::value<uint64_t>(&clock2FreqMhz), "Clock 2 frequency MHz")
    (",h", boost::program_options::value<uint64_t>(&clock3FreqMhz), "Clock 3 frequency MHz")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(clockDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (std::exception& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(clockDesc);
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(clockDesc);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  Card: %ld") % card));
  XBU::verbose(boost::str(boost::format("Region: %ld") % region));
  XBU::verbose(boost::str(boost::format("Clock1: %ld") % clock1FreqMhz));
  XBU::verbose(boost::str(boost::format("Clock2: %ld") % clock2FreqMhz));
  XBU::verbose(boost::str(boost::format("Clock3: %ld") % clock3FreqMhz));


  XBU::error("COMMAND BODY NOT IMPLEMENTED.");
  // TODO: Put working code here
}

