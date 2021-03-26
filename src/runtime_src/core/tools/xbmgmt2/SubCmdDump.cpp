/**
 * Copyright (C) 2021 Xilinx, Inc
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
#include "SubCmdDump.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/common/query_requests.h"

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// ------ L O C A L   F U N C T I O N S ---------------------------------------

static void
flash_dump(std::ofstream& /*fOutput*/)
{
  XBU::verbose("Option: flash");
  // Sample output:
  //   Output file: foo.bin
  //   Flash Size: 0x222 (Mbits)  
  //   <Progress Bar>

  // TO-DO
}

static void
config_dump(std::ofstream& /*fOutput*/)
{
  XBU::verbose("Option: config");
  //TO-DO
}

SubCmdDump::SubCmdDump(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("dump", 
             "Dump out the contents of the specified option")
{
  const std::string longDescription = "Dump out the contents of the specified option.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

// ----- C L A S S   M E T H O D S -------------------------------------------

void
SubCmdDump::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: dump");
  // -- Retrieve and parse the subcommand options -----------------------------
  std::vector<std::string> devices;
  std::string output = "";
  bool flash = false;
  bool config = false;
  bool help = false;

  po::options_description commonOptions("Common Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(devices)>(&devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("config,c", boost::program_options::bool_switch(&config), "Dumps the output of system configuration.")
    ("flash,f", boost::program_options::bool_switch(&flash), "Dumps the output of programmed system image.")
    ("output,o", boost::program_options::value<decltype(output)>(&output), "Direct the output to the given file")
    ("help,h", boost::program_options::bool_switch(&help), "Help to use this sub-command")
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
    return;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose("SubCmd: Dump");

  // -- process "device" option -----------------------------------------------
  XBU::verbose("Option: device");
  for (auto & str : devices)
    XBU::verbose(std::string(" ") + str);

  if(devices.empty()) {
    std::cerr << "ERROR: Please specify a single device using --device option" << "\n\n";
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;  // The collection of devices to examine
  for (const auto & deviceName : devices) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

  try {
    XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    return;
  }

  // enforce 1 device specification
  if(deviceCollection.size() != 1) {
    std::cerr << "ERROR: Please specify a single device. Multiple devices are not supported" << "\n\n";
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- process "output" option -----------------------------------------------
  // Output file
  XBU::verbose("Option: output: " + output);
  
  if (output.empty()) {
    std::cerr << "ERROR: Please specify an output file using --output option" << "\n\n";
    printHelp(commonOptions, hiddenOptions);
    return;
  }
  if (!output.empty() && boost::filesystem::exists(output)) {
    std::cerr << boost::format("Output file already exists: '%s'") % output << "\n\n";
    return;
  }
    
  std::ofstream fOutput;
  fOutput.open(output, std::ios::out | std::ios::binary);
  if (!fOutput.is_open()) {
    std::cerr << boost::format("ERROR: Unable to open the file '%s' for writing.") % output << "\n\n";
    return;
  }

  //decide the contents of the dump file
  if(flash)
    flash_dump(fOutput);
  else if (config)
    config_dump(fOutput);
  else {
    std::cerr << "ERROR: Please specify a valid option to determine the type of dump" << "\n\n";
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  fOutput.close();
}
