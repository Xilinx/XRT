/**
 * Copyright (C) 2021-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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
#include "SubCmdConfigure.h"
#include "OO_HostMem.h"
#include "OO_P2P.h"

#include "common/system.h"
#include "common/device.h"

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

SubCmdConfigure::SubCmdConfigure(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("configure", 
             "Device and host configuration")
{
  const std::string longDescription = "Device and host configuration.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}


void
SubCmdConfigure::execute(const SubCmdOptions& _options) const
{
  // -- Common top level options ---
  bool help = false;

  po::options_description commonOptions("Common Options"); 
  commonOptions.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options"); 

  // -- Define the supporting option options ----
  SubOptionOptions subOptionOptions;
  subOptionOptions.emplace_back(std::make_shared<OO_HostMem>("host-mem"));
  subOptionOptions.emplace_back(std::make_shared<OO_P2P>("p2p"));

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

  po::positional_options_description positionals;

  // =========== Process the options ========================================
  po::variables_map vm;
  // Used for the suboption arguments
  auto topOptions = process_arguments(vm, _options, commonOptions, hiddenOptions, positionals, subOptionOptions, false);

  // Mutual DRC
  for (unsigned int index1 = 0; index1 < subOptionOptions.size(); ++index1)
    for (unsigned int index2 = index1 + 1; index2 < subOptionOptions.size(); ++index2)
      conflictingOptions(vm, subOptionOptions[index1]->longName(), subOptionOptions[index2]->longName());


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
    if (help) {
      printHelp(commonOptions, hiddenOptions, subOptionOptions);
      return;
    }
    // If help was not requested and additional options dont match we must throw to prevent
    // invalid positional arguments from passing through without warnings
    std::cerr << "ERROR: Suboption missing" << std::endl;
    printHelp(commonOptions, hiddenOptions, subOptionOptions);
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // 2) Process the top level options
  if (help)
    topOptions.push_back("--help");

  optionOption->setGlobalOptions(getGlobalOptions());
  
  // Execute the option
  optionOption->execute(topOptions);
}
