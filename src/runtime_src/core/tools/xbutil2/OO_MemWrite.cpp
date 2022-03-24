/**
 * Copyright (C) 2020-2022 Licensed under the Apache License, Version
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
    , m_count()
    , m_fill("")
    , m_help(false)

{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device)->multitoken()->required(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("input,i", boost::program_options::value<decltype(m_inputFile)>(&m_inputFile), "Input file")
    ("address", boost::program_options::value<decltype(m_baseAddress)>(&m_baseAddress)->required(), "Base address to start from")
    ("size", boost::program_options::value<decltype(m_sizeBytes)>(&m_sizeBytes), "Block size (bytes) to write")
    ("count", boost::program_options::value<decltype(m_count)>(&m_count)->default_value(1), "Number of blocks to write")
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
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  //-- Working variables
  std::shared_ptr<xrt_core::device> device;
  long long addr = 0, size = 0;
  unsigned int fill_byte = 'J';
  std::ifstream *input_stream;
  int count = 0;

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

    // check option combinations
    // mutually exclusive options
    if (!m_inputFile.empty() && !m_fill.empty())
      throw xrt_core::error("Please specify either '--input' or '--fill'");
    // size must be specified with fill
    if (!m_fill.empty() && m_sizeBytes.empty())
      throw xrt_core::error("Please specify '--size' with '--fill'");


    //-- input file
    if (!m_inputFile.empty() && !boost::filesystem::exists(m_inputFile))
      throw xrt_core::error((boost::format("Input file does not exist: '%s'") % m_inputFile).str());
    
    input_stream = new std::ifstream(m_inputFile, std::ios::binary);
    if(!input_stream)
      throw xrt_core::error("Unable to open input file");

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
    if(!m_sizeBytes.empty())
      size = std::stoll(m_sizeBytes, nullptr, 0);
  }
  catch(const std::invalid_argument&) {
    std::cerr << boost::format("ERROR: '%s' is an invalid argument for '--size'\n") % m_sizeBytes;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // --count
  //if count is unspecified, calculate based on the file size and block size
  if(vm["count"].defaulted()) {
    input_stream->seekg(0, input_stream->end);
    int length = input_stream->tellg();
    if (size == 0) // update size
      size = length;
    count = static_cast<int>(std::ceil(length / size));
    input_stream->seekg(0, input_stream->beg);
  }
  else
    count = m_count;

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
  XBU::verbose(boost::str(boost::format("Block count: %s") % count));
  XBU::verbose(boost::str(boost::format("Fill pattern: %s") % m_fill));
  XBU::verbose(boost::str(boost::format("Input File: %s") % m_inputFile));

  //write mem
  XBU::xclbin_lock xclbin_lock(device);
  
  try {
    for(int c = 0; c < count; c++) {
      if(!m_fill.empty())
        xrt_core::mem_write(device.get(), addr, size, fill_byte);
      else {
        std::vector<char> buffer(size);
        auto input_size = input_stream->read(buffer.data(), size).gcount();
        xrt_core::mem_write(device.get(), addr, size, buffer);
        if (input_size != size)
          break; // partial read and break the loop
      }
      addr +=size;
    }
  } catch(const xrt_core::error& e) {
    std::cerr << e.what() << std::endl;
    return;
  }
  std::cout << "Memory write succeeded" << std::endl;
}

