/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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
#include "OptionOptions.h"
#include <iostream>
#include <boost/format.hpp>

#include "core/common/error.h"
#include "XBUtilitiesCore.h"
#include "XBHelpMenusCore.h"
namespace XBU = XBUtilities;
namespace po = boost::program_options;

OptionOptions::OptionOptions( const std::string & longName,
                              bool isHidden,
                              const std::string & description)
  : m_executable("<unknown>")
  , m_command("<unknown>")
  , m_longName(longName)
  , m_isHidden(isHidden)
  , m_description(description)
  , m_extendedHelp("")
  , m_defaultOptionValue(false)
{
  m_selfOption.add_options()(m_longName.c_str(), boost::program_options::bool_switch(&m_defaultOptionValue)->required(), m_description.c_str());
  m_optionsDescription.add(m_selfOption);
}

OptionOptions::OptionOptions( const std::string & longName,
                              const std::string & shortName,
                              const std::string & optionDescription,
                              const boost::program_options::value_semantic* optionValue,
                              const std::string & valueDescription,
                              bool isHidden
                              )
  : m_executable("<unknown>")
  , m_command("<unknown>")
  , m_longName(longName)
  , m_shortName(shortName)
  , m_isHidden(isHidden)
  , m_description(optionDescription)
  , m_extendedHelp("")
{
  m_selfOption.add_options()(optionNameString().c_str(), optionValue, valueDescription.c_str());
  m_optionsDescription.add(m_selfOption);
}

void 
OptionOptions::printHelp() const
{
  XBU::report_subcommand_help( m_executable,
                               m_command,
                               m_description, m_extendedHelp, 
                               m_optionsDescription, m_optionsHidden, 
                               m_positionalOptions, m_globalOptions);
}

std::vector<std::string>
OptionOptions::process_arguments( boost::program_options::variables_map& vm,
                                  const SubCmdOptions& options,
                                  bool validate_arguments) const
{
  po::options_description all_options("All Options");
  all_options.add(m_optionsDescription);
  all_options.add(m_optionsHidden);

  try {
    po::command_line_parser parser(options);
    return XBU::process_arguments(vm, parser, all_options, m_positionalOptions, validate_arguments);
  } catch(boost::program_options::error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

