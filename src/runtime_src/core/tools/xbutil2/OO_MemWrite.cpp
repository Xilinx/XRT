/**
 * Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
 *
 * Licensed under the Apache License, Version
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
#include <math.h>

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_MemWrite::OO_MemWrite( const std::string &_longName, bool _isHidden)
    : OptionOptions(_longName, _isHidden, "Write to a given memory address")
    , m_device({})
    , m_inputFile("")
    , m_baseAddress("")
    , m_sizeBytes("")
    , m_count(0)
    , m_fill("")
    , m_help(false)

{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device)->multitoken()->required(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("input,i", boost::program_options::value<decltype(m_inputFile)>(&m_inputFile), "Input file")
    ("address", boost::program_options::value<decltype(m_baseAddress)>(&m_baseAddress)->required(), "Base address to start from")
    ("size", boost::program_options::value<decltype(m_sizeBytes)>(&m_sizeBytes), "Block size (bytes) to write")
    ("count", boost::program_options::value<decltype(m_count)>(&m_count), "Number of blocks to write")
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
  XBU::verbose("SubCommand option: write mem");

  // Parse sub-command ...
  po::variables_map vm;
  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).positional(m_positionalOptions).run(), vm);
    po::notify(vm); // Can throw
  }
  catch (po::error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Honor help option first
  if (m_help) {
    printHelp();
    return;
  }

  // check option combinations
  // mutually exclusive options
  if (!m_inputFile.empty() && !m_fill.empty())
    throw xrt_core::error(std::errc::operation_canceled, "Please specify either '--input' or '--fill'");

  //-- Working variables
  std::shared_ptr<xrt_core::device> device;

  //-- Device
  if (m_device.size() > 1)
    throw xrt_core::error(std::errc::operation_canceled, "Multiple devices not supported. Please specify a single device");

  try {
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

  // Validate the given address option
  uint64_t addr = 0;
  try {
    //-- base address
    addr = std::stoull(m_baseAddress, nullptr, 0);
  }
  catch (const std::invalid_argument&) {
    std::cerr << boost::format("ERROR: '%s' is an invalid argument for '--address'\n") % m_baseAddress;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Validate the number of bytes to be written if defined
  // This does not need to be defined for the --input option path
  uint64_t size = 0;
  try {
    if (!m_sizeBytes.empty()) {
      size = XBUtilities::string_to_bytes(m_sizeBytes);
      if (size <= 0)
        throw xrt_core::error(std::errc::operation_canceled, "Size must be greater than 0");
    }
  }
  catch(const xrt_core::error& e) {
    std::cerr << boost::format("Value supplied to --size is invalid: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Parse the input option path
  if (!m_inputFile.empty()) {
    // Verify that the file exists and is not a directory
    if (!boost::filesystem::exists(m_inputFile) && boost::filesystem::is_regular_file(m_inputFile))
      throw xrt_core::error(std::errc::operation_canceled, (boost::format("Input file does not exist: '%s'") % m_inputFile).str());

    // Open the input file stream after validating the file path and name
    std::ifstream input_stream(m_inputFile, std::ios::binary);
    // If count is unspecified, calculate based on the file size and block size
    int count = 0;
    if (vm["count"].defaulted()) {
      input_stream.seekg(0, input_stream.end);
      int length = input_stream.tellg();
      if (m_sizeBytes.empty()) // update size
        size = length;
      count = static_cast<int>(std::ceil(length / size));
      input_stream.seekg(0, input_stream.beg);
    }
    else
      count = m_count;

    if (count <= 0)
      throw xrt_core::error(std::errc::operation_canceled, "Number of blocks greater than 0");

    // Logging information
    XBU::verbose(boost::str(boost::format("Device: %s") % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device))));
    XBU::verbose(boost::str(boost::format("Address: %s") % addr));
    XBU::verbose(boost::str(boost::format("Size: %llu") % size));
    XBU::verbose(boost::str(boost::format("Block count: %d") % count));
    XBU::verbose(boost::str(boost::format("Input File: %s") % m_inputFile));
    XBU::verbose(boost::str(boost::format("Bytes to write: %lld") % 5));

    // Write to device memory
    XBU::xclbin_lock xclbin_lock(device);
    
    try {
      for (int c = 0; c < count; c++) {
        XBU::verbose(boost::str(boost::format("[%d / %d] Writing to Address: %s, Size: %llu bytes") % c % count % addr % size));
        std::vector<char> buffer(size);
        // gcount will only return a value >= 0
        uint64_t input_size = static_cast<uint64_t>(input_stream.read(buffer.data(), size).gcount());
        xrt_core::mem_write(device.get(), addr, size, buffer);
        if (input_size != size)
          break; // partial read and break the loop
        addr += size;
      }
    } catch (const xrt_core::error& e) {
      std::cerr << e.what() << std::endl;
      return;
    }
    std::cout << "Memory write succeeded" << std::endl;

    return;
  }

  // Parse the fill option path
  if(!m_fill.empty()) {
    // Validate the block count. This cannot be done as the --input path may create its own block count
    if (m_count <= 0)
      throw xrt_core::error(std::errc::operation_canceled, "Number of blocks must be greater than 0");
    
    // Validate the number of bytes to write
    if (m_sizeBytes.empty())
      throw xrt_core::error(std::errc::operation_canceled, "Value required for --size when using --fill");

    unsigned int fill_byte = 'J';
    try {
      //-- fill pattern
      fill_byte = std::stoi(m_fill, nullptr, 0);
    }
    catch(const std::invalid_argument&) {
      std::cerr << boost::format("ERROR: '%s' is an invalid argument for '--fill'. Please specify a value between 0 and 255\n") % m_fill;
      throw xrt_core::error(std::errc::operation_canceled);
    }

    // Logging information
    XBU::verbose(boost::str(boost::format("Device: %s") % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device))));
    XBU::verbose(boost::str(boost::format("Address: %s") % addr));
    XBU::verbose(boost::str(boost::format("Size: %llu") % size));
    XBU::verbose(boost::str(boost::format("Block count: %s") % m_count));
    XBU::verbose(boost::str(boost::format("Fill pattern: %s") % m_fill));
    // XBU::verbose(boost::str(boost::format("Bytes to write: %ll") % (m_count * size)));

    // Write to device memory
    XBU::xclbin_lock xclbin_lock(device);
    
    try {
      // Generate the fill vector
      std::vector<char> buffer(size);
      std::fill(buffer.begin(), buffer.end(), fill_byte);
      // Write the vector to the device
      for (int c = 0; c < m_count; c++) {
        XBU::verbose(boost::str(boost::format("[%d / %d] Writing to Address: %s, Size: %s bytes") % c % m_count % addr % size));
        xrt_core::mem_write(device.get(), addr, size, buffer);
        addr += size;
      }
    } catch (const xrt_core::error& e) {
      std::cerr << e.what() << std::endl;
      return;
    }
    std::cout << "Memory write succeeded" << std::endl;

    return;
  }

  std::cerr << "No valid options provided\n";
  printHelp();
  xrt_core::error(std::errc::operation_canceled);
}

