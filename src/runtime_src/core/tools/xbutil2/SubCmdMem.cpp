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
#include "SubCmdMem.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ======= R E G I S T E R   T H E   S U B C O M M A N D ======================
#include "tools/common/SubCmd.h"
static const unsigned int registerResult = 
                    register_subcommand("mem", 
                                        "Memory write tests.",
                                        subCmdMem);
// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------




// ------ F U N C T I O N S ---------------------------------------------------

int subCmdMem(const std::vector<std::string> &_options)
// Reference Command: mem --read [-d card] [-a [0x]start_addr] [-i size_bytes] [-o output filename]
//                    mem --write [-d card] [-a [0x]start_addr] [-i size_bytes] [-e pattern_byte]
//                    Read 256 bytes from DDR starting at 0x1000 into file read.out\n";
//                      xbutil mem --read -a 0x1000 -i 256 -o read.out
//                      - Default values for address is 0x0, size is DDR size and file is memread.out
//                    Write 256 bytes to DDR starting at 0x1000 with byte 0xaa
//                      xbutil mem --write -a 0x1000 -i 256 -e 0xaa
//                      - Default values for address is 0x0, size is DDR size and pattern is 0x0

{
  XBU::verbose("SubCommand: mem");
  // -- Retrieve and parse the subcommand options -----------------------------
  bool bRead = false;
  bool bWrite = false;
  uint64_t card = 0;
  std::string sStartAddr;
  std::string sSizeBytes;
  std::string sOutputFile;
  std::string sPatternBytes;
  bool help = false;

  po::options_description memDesc("mem options");
  memDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    ("read", boost::program_options::bool_switch(&bRead), "Read operation")
    ("write", boost::program_options::bool_switch(&bWrite), "Write operation")
    (",c", boost::program_options::value<uint64_t>(&card), "Card to be examined")
    (",a", boost::program_options::value<std::string>(&sStartAddr), "Start Address")
    (",i", boost::program_options::value<std::string>(&sSizeBytes), "Size bytes")
    (",o", boost::program_options::value<std::string>(&sOutputFile), "Output File")
    (",e", boost::program_options::value<std::string>(&sPatternBytes), "Pattern bytes")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(memDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << memDesc << std::endl;

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    std::cout << memDesc << std::endl;
    return 0;
  }

  // -- Do some DRC checks here --------------------------------------------

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(XBU::format(" Read Operation: %d", bRead));
  XBU::verbose(XBU::format("Write Operation: %d", bWrite));
  XBU::verbose(XBU::format("           Card: %ld", card));
  XBU::verbose(XBU::format("  Start Address: %s", sStartAddr.c_str()));
  XBU::verbose(XBU::format("     Size Bytes: %s", sSizeBytes.c_str()));
  XBU::verbose(XBU::format("    Output File: %s", sOutputFile.c_str()));
  XBU::verbose(XBU::format("        Pattern: %s", sPatternBytes.c_str()));


  XBU::error("COMMAND BODY NOT IMPLEMENTED.");
  // TODO: Put working code here

  return registerResult;
}

