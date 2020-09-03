/**
 * Copyright (C) 2020 Licensed under the Apache License, Version
 * 2.0 (the "License"). You may not use this file except in
 * compliance with the License. A copy of the License is located
 * at
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
#include "OO_MemWrite.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_MemWrite::OO_MemWrite( const std::string &_longName, bool _isHidden)
    : OptionOptions(_longName, _isHidden, "Write to a given memory address")
    , m_device("")
    , m_baseAddress("")
    , m_sizeBytes("")
    , m_fill("")
    , m_inputFile("")
    , m_help(false)

{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("address", boost::program_options::value<decltype(m_baseAddress)>(&m_baseAddress)->required(), "Base address to start from")
    ("size", boost::program_options::value<decltype(m_sizeBytes)>(&m_sizeBytes)->required(), "Size (bytes) to write")
    ("fill,f", boost::program_options::value<decltype(m_fill)>(&m_fill), "The byte value to fill the memory with")
    ("input,i", boost::program_options::value<decltype(m_inputFile)>(&m_inputFile), "The binary file to read from")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("address", 1 /* max_count */).
    add("size", 1 /* max_count */)
  ;
}

void
OO_MemWrite::execute(const SubCmdOptions& /*_options*/) const
{
  printHelp();
}

