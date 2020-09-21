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
#include "SubCmdProgram.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "xrt.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/error.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdProgram::SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("program", 
             "Download the acceleration program to a given device")
{
  const std::string longDescription = "Programs the given acceleration image into the device's shell.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdProgram::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: program");
  // -- Retrieve and parse the subcommand options -----------------------------
  unsigned int card = 0;
  std::string xclbin;
  bool help = false;

  po::options_description commonOptions("Common Options");
  commonOptions.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    ("device,d", boost::program_options::value<unsigned int>(&card), "Card to be examined")
    ("program,p", boost::program_options::value<std::string>(&xclbin), "The xclbin image to load")
  ;

  po::options_description hiddenOptions("Hidden Options");

  po::options_description allOptions("All Options");  
  allOptions.add(commonOptions);
  allOptions.add(hiddenOptions);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(allOptions).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(commonOptions, hiddenOptions);

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  Card: %ld") % card));
  XBU::verbose(boost::str(boost::format("XclBin: %s") % xclbin.c_str()));


  if (!xclbin.empty()) {
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

    auto device = xrt_core::get_userpf_device(card);
    auto hdl = device->get_device_handle();
    if (auto err = xclLoadXclBin(hdl,reinterpret_cast<const axlf*>(raw.data())))
      throw xrt_core::error(err,"Could not program device" + std::to_string(card));

    std::cout << "INFO: xbutil2 program succeeded.\n";
    return;
  }
  std::cout << "\nERROR: Missing program operation. No action taken.\n\n";
  printHelp(commonOptions, hiddenOptions);
}
