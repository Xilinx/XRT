// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
#include "OO_MemRead.h"

// Local - Include Files
#include "core/common/memaccess.h"
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_MemRead::OO_MemRead( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Read from the given memory address" )
    , m_device("")
    , m_baseAddress("")
    , m_sizeBytes("")
    , m_count(0)
    , m_outputFile("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device)->required(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("output,o", boost::program_options::value<decltype(m_outputFile)>(&m_outputFile)->required(), "Output file")
    ("address", boost::program_options::value<decltype(m_baseAddress)>(&m_baseAddress)->required(), "Base address to start from")
    ("size", boost::program_options::value<decltype(m_sizeBytes)>(&m_sizeBytes)->required(), "Size (bytes) to read")
    ("count", boost::program_options::value<decltype(m_count)>(&m_count)->default_value(1), "Number of blocks to read")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("address", 1 /* max_count */).
    add("size", 1 /* max_count */)
  ;
}

void
OO_MemRead::execute(const SubCmdOptions& _options) const
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
  long long addr = 0;

  try {
    // Find device of interest
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);

    //-- Output file
    if (!m_outputFile.empty() && std::filesystem::exists(m_outputFile) && !XBU::getForce())
      throw xrt_core::error((boost::format("Output file already exists: '%s'") % m_outputFile).str());

  } catch (const xrt_core::error&) {
    printHelp();
    throw;
  }

  if (m_count <= 0)
    XBU::throw_cancel("Please specify a number of blocks greater than zero");

  try {
    //-- base address
    addr = std::stoll(m_baseAddress, nullptr, 0);
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

  XBU::verbose(boost::str(boost::format("Device: %s") % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device))));
  XBU::verbose(boost::str(boost::format("Address: %s") % addr));
  XBU::verbose(boost::str(boost::format("Size: %llu") % size));
  XBU::verbose(boost::str(boost::format("Block count: %d") % m_count));
  XBU::verbose(boost::str(boost::format("Output file: %s") % m_outputFile));
  XBU::verbose(boost::str(boost::format("Bytes to read: %lld") % (m_count * size)));

  //read mem
  XBU::xclbin_lock xclbin_lock(device.get());

  // Open the output file and write the data as we receive it
  std::ofstream out_file(m_outputFile, std::ofstream::out | std::ofstream::binary | std::ofstream::app);
  for(decltype(m_count) running_count = 0; running_count < m_count; running_count++) {
    XBU::verbose(boost::str(boost::format("[%d / %d] Reading from Address: %s, Size: %s bytes") % running_count % m_count % addr % size));
    // Get the output from the device
    std::vector<char> data = xrt_core::device_mem_read(device.get(), addr, size);
    // Write output to the given file
    out_file.write(data.data(), data.size());
    if ((out_file.rdstate() & std::ifstream::failbit) != 0)
      throw std::runtime_error("Error writing to output file");
    // increment the starting address by the number of bytes read
    addr += data.size();
  }
  out_file.close();

  std::cout << "Memory read succeeded" << std::endl;
}
