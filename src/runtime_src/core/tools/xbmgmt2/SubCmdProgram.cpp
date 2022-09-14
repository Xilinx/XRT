// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdProgram.h"
#include "OO_UpdateBase.h"

#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
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
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

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
    auto value = xrt_core::query::flush_default_only::value_type(boot);
    xrt_core::device_update<xrt_core::query::boot_partition>(device, value);
    std::cout << "Performing hot reset..." << std::endl;
    auto hot_reset = XBU::str_to_reset_obj("hot");
    device->reset(hot_reset);
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

SubCmdProgram::SubCmdProgram(const std::string& executable, bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("program", "Update image(s) for a given device")
    , m_device("")
    , m_help(false)
{
  const std::string longDescription = "Updates the image(s) for a given device.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
  setExecutableName(executable);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  addSubOption(std::make_shared<OO_UpdateBase>("base", "b"));
  addSubOption(std::make_shared<OO_UpdateShell>("shell", "s"));
  addSubOption(std::make_shared<OO_FactoryReset>("revert-to-golden"));
  addSubOption(std::make_shared<OO_UpdateXclbin>("user", "u"));
  addSubOption(std::make_shared<OO_ChangeBoot>("boot", "", true));
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
  std::string flashType;
  bool revertToGolden = false;
  bool help = false;

  po::options_description commonOptions("Common Options");
  commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(device_str)>(&device_str), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("shell,s", boost::program_options::value<decltype(plp)>(&plp), "The partition to be loaded.  Valid values:\n"
                                                                      "  Name (and path) of the partition.")
    ("user,u", boost::program_options::value<decltype(xclbin)>(&xclbin), "The xclbin to be loaded.  Valid values:\n"
                                                                      "  Name (and path) of the xclbin.")
    ("revert-to-golden", boost::program_options::bool_switch(&revertToGolden), "Resets the FPGA PROM back to the factory image. Note: The Satellite Controller will not be reverted for a golden image does not exist.")
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
  ;

  po::options_description hiddenOptions("Hidden Options");
  hiddenOptions.add_options()
    ("flash-type", boost::program_options::value<decltype(flashType)>(&flashType),
      "Overrides the flash mode. Use with caution.  Valid values:\n"
      "  ospi\n"
      "  ospi_versal")
    ("boot", boost::program_options::value<decltype(boot)>(&boot)->implicit_value("default"),
    "RPU and/or APU will be booted to either partition A or partition B.  Valid values:\n"
    "  DEFAULT - Reboot RPU to partition A\n"
    "  BACKUP  - Reboot RPU to partition B\n")
  ;

  po::positional_options_description positionals;

  SubOptionOptions subOptionOptions;
  subOptionOptions.emplace_back(std::make_shared<OO_UpdateBase>("base"));

  for (auto & subOO : subOptionOptions) {
    if (subOO->isHidden()) 
      hiddenOptions.add_options()(subOO->longName().c_str(), subOO->description().c_str());
    else
      commonOptions.add_options()(subOO->longName().c_str(), subOO->description().c_str());
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
    printHelp(commonOptions, hiddenOptions);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  // XBU::verbose(boost::str(boost::format("  Base: %s") % update));

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

  // Populate flash type. Uses board's default when passing an empty input string.
  if (!flashType.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
        "Overriding flash mode is not recommended.\nYou may damage your device with this option.");
  }
  Flasher working_flasher(device->get_device_id());
  auto flash_type = working_flasher.getFlashType(flashType);

  if (vm.count("base") != 0) {
    subOptionOptions[0]->execute(_options);
    return;
  }

  // Check to see if help was requested or no command was found
  if (m_help) {
    printHelp();
    return;
  }

  std::cout << "\nERROR: Missing operation. No action taken.\n\n";
  printHelp();
  throw xrt_core::error(std::errc::operation_canceled);
}
