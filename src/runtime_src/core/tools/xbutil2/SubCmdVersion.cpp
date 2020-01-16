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
#include "SubCmdVersion.h"
#include "common/system.h"
#include "gen/version.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

// ------ L O C A L   F U N C T I O N S ---------------------------------------

void reportVersions()
{
  // Report Build version information
  xrt::version::print(std::cout);

  // Get and report XOCL build information
  boost::property_tree::ptree xrt_pt;
  xrt_core::get_xrt_info(xrt_pt);

  std::cout.width(26);
  std::cout << std::internal
            << "XOCL: "
            << xrt_pt.get<std::string>( "xocl", "---Not Defined--")
            << std::endl;

  std::cout.width(26);
  std::cout << std::internal
            << "XCLMGMT: "
            << xrt_pt.get<std::string>( "xclmgmt", "---Not Defined--")
            << std::endl;
}

// ----- C L A S S   M E T H O D S -------------------------------------------
SubCmdVersion::SubCmdVersion(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("version", 
             "Reports the version of the build, OS, and drivers (if present)")
{
  const std::string longDescription = "<add long description>";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdVersion::execute(const SubCmdOptions& _options) const
// Reference Command:  version
{
  XBU::verbose("SubCommand: version");
  // -- Retrieve and parse the subcommand options -----------------------------

  bool bHelp = false;

  po::options_description versionDesc("version options");
  versionDesc.add_options()
    ("help", boost::program_options::bool_switch(&bHelp), "Help to use this sub-command")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(versionDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(versionDesc);

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (bHelp == true)  {
    printHelp(versionDesc);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  reportVersions();

  return;
}
