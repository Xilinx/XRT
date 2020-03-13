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
#include "OptionOptions.h"
#include <iostream>
#include <boost/format.hpp>

#include "XBUtilities.h"
#include "XBHelpMenus.h"
namespace XBU = XBUtilities;
namespace po = boost::program_options;

OptionOptions::OptionOptions( const std::string & _longName,
                              const std::string & _description)
  : m_executable("<unknown>")
  , m_command("<unknown>")
  , m_longName(_longName)
  , m_description(_description)
  , m_extendedHelp("")
{
  // Empty
}

void 
OptionOptions::printHelp() const
{
  XBU::report_subcommand_help(m_executable, m_command + " --" + m_longName, m_description, m_extendedHelp, m_optionsDescription, m_positionalOptions);
}

