/**
 * Copyright (C) 2022 Xilinx, Inc
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
#include "OO_Dump_Qspips.h"


// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream> 
#include <boost/format.hpp>
#include <map>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdDump::SubCmdDump(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("dump", 
             "Reads the image(s) for a given device for a given length and outputs the same to given file.\nIt is applicable for only QSPIPS flash.")
    , m_help(false)
{
  setLongDescription("Reads the image(s) for a given device for a given length and outputs the same to given file.\nIt is applicable for only QSPIPS flash.");
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  addSubOption(std::make_shared<OO_Dump_Qspips>("qspips"));
}


void
SubCmdDump::execute(const SubCmdOptions& _options) const
{
  // =========== Process the options ========================================
  po::variables_map vm;
  auto topOptions = process_arguments(vm, _options, false);


  // Find the subOption;
  auto optionOption = checkForSubOption(vm);

  // No suboption print help
  if (!optionOption) {
      if (m_help) {
          printHelp();
          return;
      }
      std::cerr << "\nERROR: Suboption missing" << std::endl;
      printHelp();
      throw std::errc::operation_canceled;
  }

  // 2) Process the top level options
  if (m_help)
    topOptions.push_back("--help");

  optionOption->setGlobalOptions(getGlobalOptions());
  
  // Execute the option
  optionOption->execute(topOptions);
}
