// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_MemWrite.h"
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "core/common/memaccess.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <filesystem>
#include <fstream>
#include <iostream>
#include <math.h>
#include <vector>

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_MemWrite::OO_MemWrite( const std::string &_longName, bool _isHidden)
    : OptionOptions(_longName, _isHidden, "Write to a given memory address")
    , m_inputFile("")
    , m_device("")
    , m_baseAddress("")
    , m_sizeBytes("")
    , m_count(0)
    , m_fill("")
    , m_help(false)

{
  m_optionsDescription.add_options()
    ("input,i", boost::program_options::value<decltype(m_inputFile)>(&m_inputFile), "Input file")
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device)->required(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
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
  process_arguments(vm, _options);

  if (m_count == 0)
    XBU::throw_cancel("Value for --count must be greater than 0");

  // check option combinations
  // mutually exclusive options
  if (m_inputFile.empty() && m_fill.empty())
    XBU::throw_cancel("Please specify either '--input' or '--fill'");

  //-- Working variables
  std::shared_ptr<xrt_core::device> device;

  try {
    // Find device of interest
    device = XBUtilities::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const xrt_core::error&) {
    printHelp();
    throw;
  }

  // Validate the given address option
  uint64_t addr = 0;
  try {
    //-- base address
    addr = std::stoull(m_baseAddress, nullptr, 0);
  }
  catch (const std::invalid_argument&) {
    XBU::throw_cancel(boost::format("'%s' is an invalid argument for '--address'") % m_baseAddress);
  }

  // Validate the number of bytes to be written if defined
  // This does not need to be defined for the --input option path
  uint64_t size = 0;
  try {
    if (!m_sizeBytes.empty())
      size = XBUtilities::string_to_base_units(m_sizeBytes, XBUtilities::unit::bytes);
  }
  catch (const xrt_core::error& e) {
    XBU::throw_cancel(boost::format("Value supplied to --size is invalid: %s") % e.what());
  }

  if (size == 0)
    XBU::throw_cancel("Value for --size must be greater than 0");

  // Parse the input option path
  if (!m_inputFile.empty()) {
    // Verify that the file exists and is not a directory
    if (!boost::filesystem::exists(m_inputFile) && boost::filesystem::is_regular_file(m_inputFile))
      XBU::throw_cancel(boost::format("Input file does not exist: '%s'") % m_inputFile);

    // Open the input file stream after validating the file path and name
    std::ifstream input_stream(m_inputFile, std::ios::binary);
    // If count is unspecified, calculate based on the file size and block size
    uint64_t count = m_count;
    if (vm["count"].defaulted()) {
      // tellg returns a signed value as the number of bytes. Validate the return code and
      // cast it into a useful format
      auto nonvalidated_length = std::filesystem::file_size(m_inputFile);
      if (nonvalidated_length < 0)
        throw std::runtime_error("Failed to get input file length");

      uint64_t validated_length = static_cast<uint64_t>(nonvalidated_length);

      if (m_sizeBytes.empty()) // update size
        size = validated_length;

      // Set count such that the entire input stream is written to the device
      // Add size to the validated length to account for truncation due to division
      // If the given size is 0 set the block count to 0
      count = (validated_length + size - 1) / size;
      input_stream.seekg(0, input_stream.beg);
    }
    else
      count = m_count;

    // Logging information
    XBU::verbose(boost::format("Device: %s") % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device)));
    XBU::verbose(boost::format("Address: %s") % addr);
    XBU::verbose(boost::format("Size: %llu") % size);
    XBU::verbose(boost::format("Block count: %llu") % count);
    XBU::verbose(boost::format("Input File: %s") % m_inputFile);
    XBU::verbose(boost::format("Bytes to write: %lld") % 5);

    // Write to device memory
    XBU::xclbin_lock xclbin_lock(device.get());

    for (decltype(count) running_count = 0; running_count < count; running_count++) {
      XBU::verbose(boost::format("[%d / %llu] Writing to Address: %s, Size: %llu bytes") % running_count % count % addr % size);
      std::vector<char> buffer(size);
      // Populate the buffer with the data to write and get the number of bytes read
      // gcount will only return a value >= 0
      auto input_size = static_cast<uint64_t>(input_stream.read(buffer.data(), size).gcount());
      xrt_core::device_mem_write(device.get(), addr, buffer);
      if (input_size != size)
        break; // partial read and break the loop
      addr += size;
    }
    std::cout << "Memory write succeeded" << std::endl;

    return;
  }

  // Parse the fill option path
  if (!m_fill.empty()) {
    // Validate the number of bytes to write
    if (m_sizeBytes.empty())
      XBU::throw_cancel("Value required for --size when using --fill");

    char fill_byte = 'J';
    try {
      //-- fill pattern
      fill_byte = static_cast<char>(std::stoi(m_fill, nullptr, 0));
    }
    catch (const std::invalid_argument&) {
      XBU::throw_cancel(boost::format("'%s' is an invalid argument for '--fill'. Please specify a value between 0 and 255") % m_fill);
    }

    // Logging information
    XBU::verbose(boost::format("Device: %s") % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device)));
    XBU::verbose(boost::format("Address: %s") % addr);
    XBU::verbose(boost::format("Size: %llu") % size);
    XBU::verbose(boost::format("Block count: %llu") % m_count);
    XBU::verbose(boost::format("Fill pattern: %s") % m_fill);

    // Write to device memory
    XBU::xclbin_lock xclbin_lock(device.get());

    // Generate the fill vector
    std::vector<char> buffer(size);
    std::fill(buffer.begin(), buffer.end(), fill_byte);
    // Write the vector to the device
    for (decltype(m_count) running_count = 0; running_count < m_count; running_count++) {
      XBU::verbose(boost::format("[%d / %llu] Writing to Address: %s, Size: %s bytes") % running_count % m_count % addr % size);
      xrt_core::device_mem_write(device.get(), addr, buffer);
      addr += size;
    }
    std::cout << "Memory write succeeded" << std::endl;
    return;
  }

  printHelp();
  XBU::throw_cancel("No valid options provided");
}
