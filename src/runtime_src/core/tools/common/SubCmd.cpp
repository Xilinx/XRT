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
#include <iostream>
#include <boost/format.hpp>

#include "XBUtilities.h"
namespace XBU = XBUtilities;

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
SubCmd::printHelp(const boost::program_options::options_description & _optionDescription) const
{
  XBU::subcommand_help(m_executableName, m_subCmdName, m_longDescription, _optionDescription, m_exampleSyntax);
}

