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
#include "OO_MemWrite.h"
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_MemWrite::OO_MemWrite( const std::string &_longName, bool _isHidden)
    : OptionOptions(_longName, _isHidden, "Write to a given memory address")
    , m_device({})
    , m_baseAddress("")
    , m_sizeBytes("")
    , m_fill("")
    , m_help(false)

{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device)->multitoken()->required(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("address", boost::program_options::value<decltype(m_baseAddress)>(&m_baseAddress)->required(), "Base address to start from")
    ("size", boost::program_options::value<decltype(m_sizeBytes)>(&m_sizeBytes)->required(), "Size (bytes) to write")
    ("fill,f", boost::program_options::value<decltype(m_fill)>(&m_fill), "The byte value to fill the memory with")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("address", 1 /* max_count */).
    add("size", 1 /* max_count */)
  ;
}

void
OO_MemWrite::execute(const SubCmdOptions& _options) const
{
XBU::verbose("SubCommand option: read mem");

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  //-- Working variables
  std::shared_ptr<xrt_core::device> device;
  long long addr = 0, size = 0;
  unsigned int fill_byte = 'J';

  try {
    //-- Device
    if(m_device.size() > 1)
      throw xrt_core::error("Multiple devices not supported. Please specify a single device");
    
    // Collect the device of interest
    std::set<std::string> deviceNames;
    xrt_core::device_collection deviceCollection;
    for (const auto & deviceName : m_device) 
      deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));
    
    XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection); // Can throw
    // set working variable
    device = deviceCollection.front();

  } catch (const xrt_core::error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  catch (const std::runtime_error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  try {
    //-- base address
    addr = std::stoll(m_baseAddress, nullptr, 0);
  }
  catch(const std::invalid_argument&) {
    std::cerr << boost::format("ERROR: '%s' is an invalid argument for '--address'\n") % m_baseAddress;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  try {
    //-- size
    size = std::stoll(m_sizeBytes, nullptr, 0);
  }
  catch(const std::invalid_argument&) {
    std::cerr << boost::format("ERROR: '%s' is an invalid argument for '--size'\n") % m_sizeBytes;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if(!m_fill.empty()) {
    try {
      //-- fill pattern
      fill_byte = std::stoi(m_fill, nullptr, 0);
    }
    catch(const std::invalid_argument&) {
      std::cerr << boost::format("ERROR: '%s' is an invalid argument for '--fill'. Please specify a value between 0 and 255\n") % m_fill;
      throw xrt_core::error(std::errc::operation_canceled);
    }
  }

  XBU::verbose(boost::str(boost::format("Device: %s") % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device))));
  XBU::verbose(boost::str(boost::format("Address: %s") % addr));
  XBU::verbose(boost::str(boost::format("Size: %s") % size));
  XBU::verbose(boost::str(boost::format("Fill pattern: %s") % m_fill));

  //read mem
  XBU::xclbin_lock xclbin_lock(device);
  
  try {
    xrt_core::mem_write(device.get(), addr, size, fill_byte);
  } catch(const xrt_core::error& e) {
    std::cerr << e.what() << std::endl;
    return;
  }
  std::cout << "Memory write succeeded" << std::endl;
}

