/**
* Copyright (C) 2021 Xilinx, Inc
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
#include "OO_AieRegRead.h"
#include "tools/common/XBUtilities.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>

namespace po = boost::program_options;
namespace qr = xrt_core::query;

// System - Include Files
#include <iostream>

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_AieRegRead::OO_AieRegRead( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Read given aie register from given row and column" )
    , m_device("")
    , m_row(0)
    , m_col(0)
    , m_reg("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("row", po::value<decltype(m_row)>(&m_row)->required(), "Row of core tile")
    ("col", po::value<decltype(m_col)>(&m_col)->required(), "Column of core tile")
    ("reg", po::value<decltype(m_reg)>(&m_reg)->required(), "Register name to read from core tile")
    ("help,h", po::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("row", 1 /* max_count */).
    add("col", 1 /* max_count */).
    add("reg", 1 /* max_count */)
  ;
}

void
OO_AieRegRead::execute(const SubCmdOptions& _options) const
{
  
  XBU::verbose("SubCommand option: aie_reg_read");

  XBU::verbose("Option(s):");
  for (auto & aString : _options)
    XBU::verbose(std::string(" ") + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).positional(m_positionalOptions).run(), vm);
    po::notify(vm); // Can throw
  }
  catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";

    printHelp();
    return;
  }

  // Exit if neither action or device specified
  if(m_help || m_device.empty()) {
    printHelp();
    return;
  }

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  deviceNames.insert(boost::algorithm::to_lower_copy(m_device));
  
  try {
    XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    return;
  }

  for (auto& device : deviceCollection) {
    try {
      uint32_t val = xrt_core::device_query<qr::aie_reg_read>(device, m_row, m_col, m_reg);
      std::cout << boost::format("Register %s Value of Row:%d Column:%d is 0x%08x\n") % m_reg.c_str() % m_row %  m_col % val;
    } catch (const std::exception& e){
      std::cerr << boost::format("ERROR: %s\n") % e.what();
    }
  }

}

