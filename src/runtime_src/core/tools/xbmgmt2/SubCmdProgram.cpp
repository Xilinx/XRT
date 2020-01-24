/**
 * Copyright (C) 2020 Xilinx, Inc
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
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdProgram::SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("program", 
             "Update device and/or Satallite Controler (SC) firmware image for a given device")
{
  const std::string longDescription = "Updates the flash image for the device and/or the Satallite Controller (SC) firmware image for a given device.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdProgram::execute(const SubCmdOptions& _options) const
// Reference Command:  [-d card] [-r region] -p xclbin
//                     Download the accelerator program for card 2
//                       xbutil program -d 2 -p a.xclbin
{
  XBU::verbose("SubCommand: examine");

  XBU::verbose("Option(s):");
  for (auto & aString : _options) {
    std::string msg = "   ";
    msg += aString;
    XBU::verbose(msg);
  }

  // -- Retrieve and parse the subcommand options -----------------------------
  std::string device = "all";
  std::string plp = "";
  std::string update = "";
  std::string image = "";
  bool revertToGolden = false;
  bool help = false;

  po::options_description queryDesc("Options");  // Note: Boost will add the colon.
  queryDesc.add_options()
    ("device,d", boost::program_options::value<decltype(device)>(&device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.  A value of 'all' (default) indicates that every found device should be examined.")
    ("plp", boost::program_options::value<decltype(plp)>(&plp), "The partition to be loaded.  Valid values:\n"
                                                                "  Name (and path) of the partiaion.\n"
                                                                "  Parition's UUID")
    ("update", boost::program_options::value<decltype(update)>(&update), "Update the persistent images.  Value values:\n"
                                                                         "  ALL   - All images will be updated\n"
                                                                         "  FLASH - Flash image\n"
                                                                         "  SC    - Satellite controller\n")
    ("image", boost::program_options::value<decltype(image)>(&image), "Specifies an image to use used to update the persistent device(s).  Value values:\n"
                                                                      "  Name of the device\n"
                                                                      "  Name (and path) to the mcs image on disk\n"
                                                                      "  Name (and path) to the xsabin image on disk")
    ("revert-to-golden", boost::program_options::bool_switch(&revertToGolden), "Resets the FPGA PROM back to the factory image.  Note: This currently only applies to the flash image.")
    ("help,h", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(queryDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(queryDesc);
    throw; // Re-throw exception
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(queryDesc);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  // Is valid BDF value valid
}
