// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_FactoryReset.h"

// XRT - Include Files
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "flash/flasher.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBUtilitiesCore.h"
namespace XBU = XBUtilities;

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/program_options.hpp>

#ifdef _WIN32
#pragma warning(disable : 4996) //std::asctime
#endif

// ----- C L A S S   M E T H O D S -------------------------------------------
OO_FactoryReset::OO_FactoryReset(const std::string& _longName, const std::string& _shortName, bool _isHidden)
    : OptionOptions(_longName,
                    _shortName,
                    "Reset the FPGA PROM back to the factory image",
                    boost::program_options::bool_switch(&m_revertToGolden)->required(),
                    "Resets the FPGA PROM back to the factory image.\n"
                    "Note: The Satellite Controller does not have a golden image and cannot be reverted",
                    _isHidden)
    , m_device("")
    , m_flashType("")
    , m_revertToGolden(false)
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help", po::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_optionsHidden.add_options()
    ("flash-type", boost::program_options::value<decltype(m_flashType)>(&m_flashType),
    "Overrides the flash mode. Use with caution.  Valid values:\n"
    "  ospi\n"
    "  ospi_versal")
  ;
}

void
OO_FactoryReset::execute(const SubCmdOptions& _options) const
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

  // Populate flash type. Uses board's default when passing an empty input string.
  if (!m_flashType.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
        "Overriding flash mode is not recommended.\nYou may damage your device with this option.");
  }
  Flasher working_flasher(device->get_device_id());
  auto flash_type = working_flasher.getFlashType(m_flashType);

  // -- process "revert-to-golden" option ---------------------------------------
  if (m_revertToGolden) {
    XBU::verbose("Sub command: --revert-to-golden");
    bool has_reset = false;

    std::vector<Flasher> flasher_list;
    //collect information of all the devices that will be reset
    Flasher flasher(device->get_device_id());
    if (!flasher.isValid())
      xrt_core::error(boost::str(boost::format("%d is an invalid index") % device->get_device_id()));

    std::cout << boost::format("%-8s : %s %s %s \n") % "INFO" % "Resetting device ["
      % flasher.sGetDBDF() % "] back to factory mode.";
    flasher_list.push_back(flasher);

    XBUtilities::sudo_or_throw("Root privileges are required to revert the device to its golden flash image");

    //ask user's permission
    if (!XBU::can_proceed(XBU::getForce()))
      throw xrt_core::error(std::errc::operation_canceled);

    for (auto& f : flasher_list) {
      if (!f.upgradeFirmware(flash_type, nullptr, nullptr, nullptr)) {
        std::cout << boost::format("%-8s : %s %s %s\n") % "INFO" % "Shell on [" % f.sGetDBDF() % "]"
                                   " is reset successfully.";
        has_reset = true;
      }
    }

    if (!has_reset)
      return;

    std::cout << "****************************************************\n";
    std::cout << "Cold reboot machine to load the new image on device.\n";
    std::cout << "****************************************************\n";

    return;
  }

  std::cout << "\nERROR: Missing flash operation.  No action taken.\n\n";
  printHelp();
  throw xrt_core::error(std::errc::operation_canceled);
}
