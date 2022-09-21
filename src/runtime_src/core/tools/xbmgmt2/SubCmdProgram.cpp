// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdProgram.h"

#include "OO_FactoryReset.h"
#include "OO_UpdateBase.h"
#include "OO_UpdateShell.h"
#include "OO_UpdateXclbin.h"

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
// Remove linux specific code
#ifdef __linux__
#include "core/pcie/linux/scan.h"
#endif

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

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
{
  const std::string longDescription = "Updates the image(s) for a given device.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
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

  // -- Retrieve and parse the subcommand options -----------------------------
  std::string device_str;
  std::string plp;
  std::string xclbin;
  std::string boot;
  bool help = false;

  po::options_description commonOptions("Common Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(device_str)>(&device_str), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options");
  hiddenOptions.add_options()
    ("boot", boost::program_options::value<decltype(boot)>(&boot)->implicit_value("default"),
    "RPU and/or APU will be booted to either partition A or partition B.  Valid values:\n"
    "  DEFAULT - Reboot RPU to partition A\n"
    "  BACKUP  - Reboot RPU to partition B\n")
  ;

  po::positional_options_description positionals;

  SubOptionOptions subOptionOptions;
  subOptionOptions.emplace_back(std::make_shared<OO_UpdateBase>("base", "b"));
  subOptionOptions.emplace_back(std::make_shared<OO_UpdateShell>("shell", "s"));
  subOptionOptions.emplace_back(std::make_shared<OO_FactoryReset>("revert-to-golden"));
  subOptionOptions.emplace_back(std::make_shared<OO_UpdateXclbin>("user", "u"));

  for (auto & subOO : subOptionOptions) {
    if (subOO->isHidden()) 
      hiddenOptions.add_options()(subOO->optionNameString().c_str(), subOO->description().c_str());
    else
      commonOptions.add_options()(subOO->optionNameString().c_str(), subOO->description().c_str());
    subOO->setExecutable(getExecutableName());
    subOO->setCommand(getName());
  }

  // Parse sub-command ...
  po::variables_map vm;
  auto topOptions = process_arguments(vm, _options, commonOptions, hiddenOptions, positionals, subOptionOptions, false);

    // Find the subOption;
  std::shared_ptr<OptionOptions> optionOption;
  for (auto& subOO : subOptionOptions) {
    if (vm.count(subOO->longName().c_str()) != 0) {
      optionOption = subOO;
      break;
    }
  }

  if (optionOption) {
    optionOption->execute(_options);
    return;
  }

  // Check to see if help was requested or no command was found
  if (help) {
    printHelp(commonOptions, hiddenOptions, subOptionOptions);
    return;
  }

  // -- process "device" option -----------------------------------------------
  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(device_str), false /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // -- process "boot" option ------------------------------------------
  if (!boot.empty()) {
    if (boost::iequals(boot, "DEFAULT"))
      switch_partition(device.get(), 0);
    else if (boost::iequals(boot, "BACKUP"))
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
  printHelp(commonOptions, hiddenOptions, subOptionOptions);
  throw xrt_core::error(std::errc::operation_canceled);
}
