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
#include "SubCmd.h"
#include "XBHelpMenus.h"
#include <iostream>
#include <boost/format.hpp>

#include "XBUtilities.h"
namespace XBU = XBUtilities;
namespace po = boost::program_options;

SubCmd::SubCmd(const std::string & _name, 
               const std::string & _shortDescription)
  : m_executableName("")
  , m_subCmdName(_name)
  , m_shortDescription(_shortDescription)
  , m_longDescription("")
  , m_isHidden(false)
  , m_isDeprecated(false)
  , m_isPreliminary(false)
{
  // Empty
}

void
SubCmd::printHelp( const boost::program_options::options_description & _optionDescription) const
{
  boost::program_options::positional_options_description emptyPOD;
  XBUtilities::report_subcommand_help(m_executableName, m_subCmdName, m_longDescription,  m_exampleSyntax, _optionDescription, emptyPOD);
}

void
SubCmd::printHelp( const boost::program_options::options_description & _optionDescription,
                   const SubOptionOptions & _subOptionOptions) const
{
 XBUtilities::report_subcommand_help(m_executableName, m_subCmdName, m_longDescription,  m_exampleSyntax, _optionDescription, _subOptionOptions);
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

