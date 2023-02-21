// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_UpdateShell.h"

// XRT - Include Files
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "flash/flasher.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <fstream>
#include <iostream>

static void
program_plp(const xrt_core::device* dev, const std::string& partition)
{
  std::ifstream stream(partition.c_str(), std::ios_base::binary);
  if (!stream.is_open())
    throw xrt_core::error(boost::str(boost::format("Cannot open %s") % partition));

  //size of the stream
  stream.seekg(0, stream.end);
  int total_size = static_cast<int>(stream.tellg());
  stream.seekg(0, stream.beg);

  //copy stream into a vector
  std::vector<char> buffer(total_size);
  stream.read(buffer.data(), total_size);

  try {
    xrt_core::program_plp(dev, buffer, XBU::getForce());
  }
  catch (xrt_core::error& e) {
    std::cout << "ERROR: " << e.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }
  std::cout << "Programmed shell successfully" << std::endl;
}

OO_UpdateShell::OO_UpdateShell(const std::string& _longName, const std::string& _shortName, bool _isHidden)
    : OptionOptions(_longName,
                    _shortName,
                    "Update the shell partition for a 2RP platform",
                    boost::program_options::value<decltype(m_plp)>(&m_plp)->implicit_value("all")->required(),
                    "The partition to be loaded.  Valid values:\n"
                    "  Name (and path) of the partition.",
                    _isHidden)
    , m_device("")
    , m_plp("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", po::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
OO_UpdateShell::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: program");

  XBU::verbose("Option(s):");
  for (auto & aString : _options) {
    std::string msg = "   ";
    msg += aString;
    XBU::verbose(msg);
  }

  // Parse sub-command ...
  po::variables_map vm;
  auto topOptions = process_arguments(vm, _options);

  // Check to see if help was requested or no command was found
  if (m_help) {
    printHelp();
    return;
  }

  // -- process "device" option -----------------------------------------------
  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), false /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // -- process "plp" option ---------------------------------------
  if (!m_plp.empty()) {
    XBU::verbose(boost::str(boost::format("  shell: %s") % m_plp));

    Flasher flasher(device->get_device_id());
    if (!flasher.isValid())
      throw xrt_core::error(boost::str(boost::format("%d is an invalid index") % device->get_device_id()));

    if (xrt_core::device_query<xrt_core::query::interface_uuids>(device).empty())
      throw xrt_core::error("Can not get BLP interface uuid. Please make sure corresponding BLP package"
                            " is installed.");

    // Check if file exists
    if (!boost::filesystem::exists(m_plp))
      throw xrt_core::error("File not found. Please specify the correct path");

    DSAInfo dsa(m_plp);
    //TO_DO: add a report for plp before asking permission to proceed. Replace following 2 lines
    std::cout << "Programming shell on device [" << flasher.sGetDBDF() << "]..." << std::endl;
    std::cout << "Partition file: " << dsa.file << std::endl;

    for (const auto& uuid : dsa.uuids) {

      //check if plp is compatible with the installed blp
      if (xrt_core::device_query<xrt_core::query::interface_uuids>(device).front().compare(uuid) == 0) {
        XBUtilities::sudo_or_throw("Root privileges are required to load the PLP image");
        program_plp(device.get(), dsa.file);
        return;
      }
    }

    // Fall through error
    throw xrt_core::error("uuid does not match BLP");
  }

  std::cout << "\nERROR: Missing flash operation.  No action taken.\n\n";
  printHelp();
  throw xrt_core::error(std::errc::operation_canceled);
}
