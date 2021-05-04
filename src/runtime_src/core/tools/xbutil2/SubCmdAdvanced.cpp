/**
 * Copyright (C) 2020-2021 Xilinx, Inc
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
#include "SubCmdAdvanced.h"
#include "OO_MemRead.h"
#include "OO_AieRegRead.h"
#include "OO_MemWrite.h"
#include "OO_P2P.h"
#include "XBReport.h"

#include "common/system.h"
#include "common/device.h"
#include "common/xclbin_parser.h"

#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream> 

#include "common/system.h"
#include "common/device.h"
#include <boost/format.hpp>
#include <map>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdAdvanced::SubCmdAdvanced(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("advanced", 
             "Low level command operations")
{
  const std::string longDescription = "Low level command operations.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}


void
SubCmdAdvanced::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: advanced");

  // -- Common top level options ---
  bool help = false;

  po::options_description commonOptions("Common Options"); 
  commonOptions.add_options()
    ("help,h", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options"); 

  // -- Define the supporting option options ----
  SubOptionOptions subOptionOptions;
  subOptionOptions.emplace_back(std::make_shared<OO_MemRead>("read-mem"));
  subOptionOptions.emplace_back(std::make_shared<OO_MemWrite>("write-mem"));
  subOptionOptions.emplace_back(std::make_shared<OO_P2P>("p2p"));
// Only defined for embedded platform
#ifndef ENABLE_NATIVE_SUBCMDS_AND_REPORTS
  subOptionOptions.emplace_back(std::make_shared<OO_AieRegRead>("read-aie-reg"));
#endif

  for (auto & subOO : subOptionOptions) {
    if (subOO->isHidden()) 
      hiddenOptions.add_options()(subOO->longName().c_str(), subOO->description().c_str());
    else
      commonOptions.add_options()(subOO->longName().c_str(), subOO->description().c_str());
    subOO->setExecutable(getExecutableName());
    subOO->setCommand(getName());
  }

  po::options_description allOptions("All Options");
  allOptions.add(commonOptions);
  allOptions.add(hiddenOptions);

  // =========== Process the options ========================================

  // 1) Process the common top level options 
  po::parsed_options parsedCommonTop = 
    po::command_line_parser(_options).
    options(allOptions).          
    allow_unregistered().           // Allow for unregistered options
    run();                          // Parse the options

  po::variables_map vm;

  try {
    po::store(parsedCommonTop, vm);  // Can throw
    po::notify(vm);                  // Can throw (but really isn't used)

    // Multual DRC
    for (unsigned int index1 = 0; index1 < subOptionOptions.size(); ++index1) {
      for (unsigned int index2 = index1 + 1; index2 < subOptionOptions.size(); ++index2) {
        conflictingOptions(vm, subOptionOptions[index1]->longName(), subOptionOptions[index2]->longName());
      }
    }
  } catch (const std::exception & e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    printHelp(commonOptions, hiddenOptions, subOptionOptions);
    return;
  }

  // Find the subOption;
  std::shared_ptr<OptionOptions> optionOption;
  for (auto& subOO : subOptionOptions) {
    if (vm.count(subOO->longName().c_str()) != 0) {
      optionOption = subOO;
      break;
    }
  }

  // No suboption print help
  if (!optionOption) {
    printHelp(commonOptions, hiddenOptions, subOptionOptions);
    return;
  }

  // 2) Process the top level options
  std::vector<std::string> topOptions = po::collect_unrecognized(parsedCommonTop.options, po::include_positional);
  if (help) {
    topOptions.push_back("--help");
  }

  optionOption->setGlobalOptions(getGlobalOptions());
  
  // Execute the option
  optionOption->execute(topOptions);
}
