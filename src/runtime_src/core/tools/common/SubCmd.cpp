/**
 * Copyright (C) 2019 Xilinx, Inc
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

static SubCmdTable cmdTable;

unsigned int 
register_subcommand(const std::string &_subCmdName, 
                    const std::string & _description, 
                    t_subcommand _pSubCommand,
                    bool _isHidden)
{
  if (cmdTable.find(_subCmdName) != cmdTable.end()) {
    XBU::fatal(boost::str(boost::format("Sub-command '%s' already registered.") % _subCmdName));
    exit(1);
  }

  cmdTable[_subCmdName] = SubCmdEntry{_subCmdName, _description, _pSubCommand, _isHidden};
  return 0;
}

const SubCmdEntry *
getSubCmdEntry(const std::string &_sSubCmdName)
{
  if (cmdTable.find(_sSubCmdName) == cmdTable.end()) {
    return nullptr;
  }

  return &cmdTable[_sSubCmdName];
}

const std::map<const std::string, SubCmdEntry> &
getSubCmdsTable()
{
  return cmdTable;
}
