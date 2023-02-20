// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdProgram.h"
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenus.h"
#include "tools/common/ProgressBar.h"
#include "tools/common/Process.h"
namespace XBU = XBUtilities;

#include "xrt.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include "core/common/message.h"
#include "core/common/utils.h"
#include "flash/flasher.h"
#include "core/common/info_vmr.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// ---- Reports ------
#include "ReportPlatform.h"
#include "tools/common/Report.h"
#include "tools/common/ReportHost.h"

#include "OO_UpdateBase.h"
#include "OO_FactoryReset.h"

// System - Include Files
#include <atomic>
#include <chrono>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <locale>
#include <map>
#include <thread>

#ifdef _WIN32
#pragma warning(disable : 4996) //std::asctime
#endif


// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

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

static void
switch_partition(xrt_core::device* device, int boot)
{
  auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  std::cout << boost::format("Rebooting device: [%s] with '%s' partition")
                % bdf % (boot ? "backup" : "default") << std::endl;
  try {
    // sets sysfs node boot_from_back [1:backup, 0:default], then hot reset
    auto value = xrt_core::query::flush_default_only::value_type(boot);
    xrt_core::device_update<xrt_core::query::boot_partition>(device, value);

    std::cout << "Performing hot reset..." << std::endl;
    device->device_shutdown();
    device->device_online();
    std::cout << "Rebooted successfully" << std::endl;
  }
  catch (const xrt_core::query::exception& ex) {
    std::cout << "ERROR: " << ex.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
    // only available for versal devices
  }
}

}
//end anonymous namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdProgram::SubCmdProgram(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("program",
             "Update image(s) for a given device")
    , m_device("")
    , m_plp("")
    , m_xclbin("")
    , m_boot("")
    , m_help(false)

{
  const std::string longDescription = "Updates the image(s) for a given device.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("shell,s", boost::program_options::value<decltype(m_plp)>(&m_plp), "The partition to be loaded.  Valid values:\n"
                                                                      "  Name (and path) of the partition.")
    ("user,u", boost::program_options::value<decltype(m_xclbin)>(&m_xclbin), "The xclbin to be loaded.  Valid values:\n"
                                                                      "  Name (and path) of the xclbin.")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_hiddenOptions.add_options()
    ("boot", boost::program_options::value<decltype(m_boot)>(&m_boot)->implicit_value("default"),
    "RPU and/or APU will be booted to either partition A or partition B.  Valid values:\n"
    "  DEFAULT - Reboot RPU to partition A\n"
    "  BACKUP  - Reboot RPU to partition B\n")
  ;

  addSubOption(std::make_shared<OO_UpdateBase>("base", "b"));
  addSubOption(std::make_shared<OO_FactoryReset>("revert-to-golden"));
}

void
SubCmdProgram::execute(const SubCmdOptions& _options) const
// Reference Command:  [-d card] [-r region] -p xclbin
//                     Download the accelerator program for card 2
//                       xbutil program -d 2 -p a.xclbin
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
  auto topOptions = process_arguments(vm, _options, false);

  // Check for a suboption
  auto optionOption = checkForSubOption(vm);

  if (optionOption) {
    optionOption->execute(_options);
    return;
  }

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

  // -- process "user" option ---------------------------------------
  if (!m_xclbin.empty()) {
    XBU::verbose(boost::str(boost::format("  xclbin: %s") % m_xclbin));
    XBU::sudo_or_throw("Root privileges are required to download xclbin");

    std::ifstream stream(m_xclbin, std::ios::binary);
    if (!stream)
      throw xrt_core::error(boost::str(boost::format("Could not open %s for reading") % m_xclbin));

    stream.seekg(0,stream.end);
    ssize_t size = stream.tellg();
    stream.seekg(0,stream.beg);

    std::vector<char> xclbin_buffer(size);
    stream.read(xclbin_buffer.data(), size);

    auto bdf = xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
    std::cout << "Downloading xclbin on device [" << bdf << "]..." << std::endl;
    try {
      device->xclmgmt_load_xclbin(xclbin_buffer.data());
    } catch (xrt_core::error& e) {
      std::cout << "ERROR: " << e.what() << std::endl;
      throw xrt_core::error(std::errc::operation_canceled);
    }
    std::cout << boost::format("INFO: Successfully downloaded xclbin \n") << std::endl;
    return;
  }

  // -- process "boot" option ------------------------------------------
  if (!m_boot.empty()) {
    if (boost::iequals(m_boot, "DEFAULT"))
      switch_partition(device.get(), 0);
    else if (boost::iequals(m_boot, "BACKUP"))
      switch_partition(device.get(), 1);
    else {
      std::cout << "ERROR: Invalid value.\n"
                << "Usage: xbmgmt program --device='0000:00:00.0' --boot [default|backup]"
                << std::endl;
      throw xrt_core::error(std::errc::operation_canceled);
    }
    return;
  }

  std::cout << "\nERROR: Missing flash operation.  No action taken.\n\n";
  printHelp();
  throw xrt_core::error(std::errc::operation_canceled);
}
