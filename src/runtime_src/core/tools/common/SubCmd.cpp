/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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
#include "SubCmd.h"
#include <iostream>
#include <boost/format.hpp>

#include "core/common/error.h"
#include "XBUtilitiesCore.h"
#include "XBHelpMenusCore.h"
namespace XBU = XBUtilities;
namespace po = boost::program_options;

SubCmd::SubCmd(const std::string & _name, 
               const std::string & _shortDescription)
  : m_commonOptions("Common Options")
  , m_hiddenOptions("Hidden Options")
  , m_executableName("")
  , m_subCmdName(_name)
  , m_shortDescription(_shortDescription)
  , m_longDescription("")
  , m_isHidden(false)
  , m_isDeprecated(false)
  , m_isPreliminary(false)
  , m_defaultDeviceValid(true)
{
  // Empty
}

void
SubCmd::printHelp( const boost::program_options::options_description & _optionDescription,
                   const boost::program_options::options_description & _optionHidden,
                   bool removeLongOptDashes,
                   const std::string& customHelpSection) const
{
  boost::program_options::positional_options_description emptyPOD;
  XBUtilities::report_subcommand_help(m_executableName, m_subCmdName, m_longDescription,  m_exampleSyntax, _optionDescription, _optionHidden, emptyPOD, m_globalOptions, removeLongOptDashes, customHelpSection);
}

void
SubCmd::printHelp( const boost::program_options::options_description & _optionDescription,
                   const boost::program_options::options_description & _optionHidden,
                   const SubOptionOptions & _subOptionOptions) const
{
 XBUtilities::report_subcommand_help(m_executableName, m_subCmdName, m_longDescription,  m_exampleSyntax, _optionDescription, _optionHidden, _subOptionOptions, m_globalOptions);
}

void
SubCmd::printHelp() const
{
 XBUtilities::report_subcommand_help(m_executableName, m_subCmdName, m_longDescription,  m_exampleSyntax, m_commonOptions, m_hiddenOptions, m_subOptionOptions, m_globalOptions);
}

std::vector<std::string> 
SubCmd::process_arguments( po::variables_map& vm,
                           const SubCmdOptions& _options,
                           const po::options_description& common_options,
                           const po::options_description& hidden_options,
                           const po::positional_options_description& positionals,
                           const SubOptionOptions& suboptions,
                           bool validate_arguments) const
{
  po::options_description all_options("All Options");
  all_options.add(common_options);
  all_options.add(hidden_options);

  try {
    po::command_line_parser parser(_options);
    return XBU::process_arguments(vm, parser, all_options, positionals, validate_arguments);
  } catch(boost::program_options::error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp(common_options, hidden_options, suboptions);
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

std::vector<std::string>
SubCmd::process_arguments( boost::program_options::variables_map& vm,
                          const SubCmdOptions& _options,
                          bool validate_arguments) const
{
  return process_arguments(vm, _options, m_commonOptions, m_hiddenOptions, m_positionals, m_subOptionOptions, validate_arguments);
}

void 
SubCmd::conflictingOptions( const boost::program_options::variables_map& _vm, 
                            const std::string &_opt1, const std::string &_opt2) const
{
  if ( _vm.count(_opt1.c_str())  
       && !_vm[_opt1.c_str()].defaulted() 
       && _vm.count(_opt2.c_str()) 
       && !_vm[_opt2.c_str()].defaulted()) {
    std::string errMsg = boost::str(boost::format("Mutually exclusive options: '%s' and '%s'") % _opt1 % _opt2);
    throw std::logic_error(errMsg);
  }
}

void
SubCmd::addSubOption(std::shared_ptr<OptionOptions> option)
{
  option->setExecutable(getExecutableName());
  option->setCommand(getName());
  m_subOptionOptions.emplace_back(option);
}

std::shared_ptr<OptionOptions>
SubCmd::checkForSubOption(const boost::program_options::variables_map& vm) const
{
  std::shared_ptr<OptionOptions> option;
  for (auto& subOO : m_subOptionOptions) {
    if (vm.count(subOO->longName().c_str()) != 0) {
      if (!option)
        option = subOO;
      else {
        auto err_fmt = boost::format("Mutually exclusive option selected: %s %s") 
                        % subOO->longName() % option->longName();
        throw xrt_core::error(std::errc::operation_canceled, boost::str(err_fmt)); 
      }
    }
  }
  return option;
}
