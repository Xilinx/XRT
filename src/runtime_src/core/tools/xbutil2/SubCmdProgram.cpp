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
#include "SubCmdProgram.h"
#include "XBUtilities.h"
namespace XBU = XBUtilities;

#include "xrt.h"
#include "core/common/device_core.h"
#include "core/common/error.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// ======= R E G I S T E R   T H E   S U B C O M M A N D ======================
#include "SubCmd.h"
static const unsigned int registerResult = 
                    register_subcommand("program", 
                                        "Download the acceleration program to a given device",
                                        subCmdProgram);
// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------



// ------ F U N C T I O N S ---------------------------------------------------

int subCmdProgram(const std::vector<std::string> &_options)
// Reference Command:  [-d card] [-r region] -p xclbin
//                     Download the accelerator program for card 2
//                       xbutil program -d 2 -p a.xclbin


{
  XBU::verbose("SubCommand: program");
  // -- Retrieve and parse the subcommand options -----------------------------
  uint64_t card = 0;
  uint64_t region = 0;
  std::string xclbin;
  bool help = false;

  po::options_description programDesc("program options");
  programDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<uint64_t>(&card), "Card to be examined")
    (",r", boost::program_options::value<uint64_t>(&region), "Card region")
    (",p", boost::program_options::value<std::string>(&xclbin), "The xclbin image to load")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(programDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    xrt_core::send_exception_message(e.what(), "XBUTIL");
    std::cerr << programDesc << std::endl;

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    std::cout << programDesc << std::endl;
    return 0;
  }

  if (xclbin.empty())
    throw xrt_core::error("Please specify xclbin file with '-p' switch");

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(XBU::format("  Card: %ld", card));
  XBU::verbose(XBU::format("Region: %ld", region));
  XBU::verbose(XBU::format("XclBin: %s", xclbin.c_str()));

  if (region)
    throw xrt_core::error("region is not supported");

  std::ifstream stream(xclbin, std::ios::binary);
  if (!stream)
    throw std::runtime_error("could not open " + xclbin + " for reading");

  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);

  std::vector<char> raw(size);
  stream.read(raw.data(),size);

  std::string v(raw.data(),raw.data()+7);
  if (v != "xclbin2")
    throw xrt_core::error("bad binary version '" + v + "'");

  auto device = xrt_core::device_core::instance().get_device(card);
  if (auto err = device.execute(xclLockDevice))
    throw xrt_core::error(err, "Could not lock device " + std::to_string(card));
  if (auto err = device.execute(xclLoadXclBin, (reinterpret_cast<const axlf*>(raw.data()))))
    throw xrt_core::error(err,"Could not program device" + std::to_string(card));
  if (auto err = device.execute(xclUnlockDevice))
    throw xrt_core::error(err, "Could not unlock device " + std::to_string(card));

  std::cout << "INFO: xbutil2 program succeeded.\n";

  return registerResult;
}

