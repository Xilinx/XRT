// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_UpdateXclbin.h"

// XRT - Include Files
#include "core/common/query_requests.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
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

OO_UpdateXclbin::OO_UpdateXclbin(const std::string &_longName, const std::string &_shortName, bool _isHidden)
    : OptionOptions(_longName,
                    _shortName,
                    "Load an xclbin onto the FPGA",
                    boost::program_options::value<decltype(m_xclbin)>(&m_xclbin)->required(),
                    "The xclbin to be loaded.  Valid values:\n"
                    "  Name (and path) of the xclbin.",
                    _isHidden)
    , m_device("")
    , m_xclbin("")
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", po::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
OO_UpdateXclbin::execute(const SubCmdOptions &_options) const
{
  XBUtilities::verbose("SubCommand option: Update xclbin");

  XBUtilities::verbose("Option(s):");
  for (const auto &aString : _options)
    XBUtilities::verbose(" " + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), false /*inUserDomain*/);
  } catch (const std::runtime_error &e) {
    XBU::throw_cancel(e.what());
  }

  XBU::sudo_or_throw("Root privileges are required to download xclbin");

  std::ifstream stream(m_xclbin, std::ios::binary);
  if (!stream)
    throw xrt_core::error(boost::str(boost::format("Could not open %s for reading") % m_xclbin));

  auto size = std::filesystem::file_size(m_xclbin);

  std::vector<char> xclbin_buffer(size);
  stream.read(xclbin_buffer.data(), size);

  auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  std::cout << "Downloading xclbin on device [" << bdf << "]..." << std::endl;
  try {
    device->xclmgmt_load_xclbin(xclbin_buffer.data());
  } catch (xrt_core::error &e) {
    XBU::throw_cancel(e.what());
  }
  std::cout << boost::format("INFO: Successfully downloaded xclbin \n") << std::endl;
  return;
}
