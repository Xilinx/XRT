// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdProgram.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "xrt.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"


// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <filesystem>
#include <fstream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdProgram::SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("program", 
             "Download the acceleration program to a given device")
    , m_device("")
    , m_xclbin("")
    , m_help(false)
{
  setLongDescription("Programs the given acceleration image into the device's shell.");
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("user,u", boost::program_options::value<std::string>(&m_xclbin), "The name (and path) of the xclbin to be loaded")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
SubCmdProgram::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: program");
  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  // Check to see if help was requested or no command was found
  if (m_help) {
    printHelp();
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  XclBin: %s") % m_xclbin));

  // -- process "device" option -----------------------------------------------
  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // -- process "program" option -----------------------------------------------
  if (!m_xclbin.empty()) {
    std::ifstream stream(m_xclbin, std::ios::binary);
    if (!stream)
      throw xrt_core::error(boost::str(boost::format("Could not open %s for reading") % m_xclbin));

    size_t size = std::filesystem::file_size(m_xclbin);

    std::vector<char> raw(size);
    stream.read(raw.data(),size);

    std::string v(raw.data(),raw.data()+7);
    if (v != "xclbin2")
      throw xrt_core::error(boost::str(boost::format("Bad binary version '%s'") % v));

    auto hdl = device->get_device_handle();
    auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
    if (auto err = xclLoadXclBin(hdl,reinterpret_cast<const axlf*>(raw.data())))
      throw xrt_core::error(err, "Could not program device " + bdf);

    std::cout << "INFO: xbutil program succeeded on " << bdf << std::endl;
    return;
  }

  std::cout << "\nERROR: Missing program operation. No action taken.\n\n";
  printHelp();
  throw xrt_core::error(std::errc::operation_canceled);
}
