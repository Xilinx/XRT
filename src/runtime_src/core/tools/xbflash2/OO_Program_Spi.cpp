/**
 * Copyright (C) 2022 Licensed under the Apache License, Version
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
#include "OO_Program_Spi.h"
#include "tools/common/XBUtilitiesCore.h"

#include "core/pcie/tools/xbflash.qspi/firmware_image.h"
#include "core/pcie/tools/xbflash.qspi/pcidev.h"
#include "core/pcie/tools/xbflash.qspi/xspi.h"

namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

namespace {

//
void
spiCommand(po::variables_map& vm) {
    //root privileges required.
    XBU::sudo_or_throw("ERROR: root privileges required.");

    //mandatory command line args
    std::string bdf = vm.count("device") ? vm["device"].as<std::string>() : "";
    if (bdf.empty())
        throw std::runtime_error("Device not specified. Please specify a single device using --device option");

    //optional command line args
    std::string sBar = vm.count("bar") ? vm["bar"].as<std::string>() : "";
    int bar = !sBar.empty() ? std::stoi(sBar, nullptr, 0) : 0;
    std::string sBarOffset = vm.count("bar-offset") ? vm["bar-offset"].as<std::string>() : "";
    size_t baroff = !sBarOffset.empty() ? std::stoul(sBarOffset, nullptr, 0) : INVALID_OFFSET;
    bool force = vm.count("force") ? true : false;
    bool dualflash = vm.count("dual-flash") ? true : false;    

    if (vm.count("revert-to-golden")) { //spi - reset/revert-to-golden
        std::cout << "About to revert to golden image for device " << bdf << std::endl;
        if (!force && !XBU::can_proceed())
            throw std::errc::operation_canceled;

        pcidev::pci_device dev(bdf, bar, baroff);
        XSPI_Flasher xspi(&dev, dualflash);
        if (xspi.revertToMFG())
            throw std::runtime_error("Flash type - spi, Reset failed.");
        return;
    }

    if (vm.count("image")) { // spi - flash/image
        std::vector <std::string> primary_file;
        primary_file = vm["image"].as<std::vector<std::string>>();
        if (primary_file.size() == 2)
            dualflash = true;

        std::cout << "Preparing to program flash on device: " << bdf << std::endl;
        if (!force && !XBU::can_proceed())
            throw std::errc::operation_canceled;

        pcidev::pci_device dev(bdf, bar, baroff);
        XSPI_Flasher xspi(&dev, dualflash);

        if (!dualflash) {
            firmwareImage pri(primary_file[0].c_str());
            if (pri.fail())
                throw std::runtime_error("firmwareImage object creation failed.");
            if (xspi.xclUpgradeFirmware1(pri))
                throw std::runtime_error("spi flash failed.");
        }
        else {
            firmwareImage pri(primary_file[0].c_str());
            firmwareImage sec(primary_file[1].c_str());
            if (pri.fail() || sec.fail())
                throw std::runtime_error("firmwareImage object creation failed.");
            if (xspi.xclUpgradeFirmware2(pri, sec))
                throw std::runtime_error("spi flash failed.");
        }
        return;
    }
    throw std::runtime_error("Missing program operation.No action taken.");
}
} //end namespace 

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Program_Spi::OO_Program_Spi( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Flash type - spi")
    , m_help(false)
{

  m_optionsDescription.add_options()
      ("device,d", po::value<std::string>(), "The \"Bus:Device.Function\" (e.g., 0000:d8:00.0) device of interest.\n")
      ("dual-flash,u", "Specifies if the card is dual flash supported.\n")
      ("bar,b", po::value<std::string>(), "BAR-index, default is 0.\n")
      ("bar-offset,s", po::value<std::string>(), "BAR-offset-for-QSPIPS, default is 0x40000.\n")
      ("image,i", po::value<std::vector<std::string>>(), "Specifies MCS or BOOT.BIN image path to update the persistent device.\n")
      ("revert-to-golden,r", "Resets the FPGA PROM back to the factory image.\n")
      ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
OO_Program_Spi::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand option: Flash type - spi");
  XBU::verbose("Option(s):");
  for (auto & aString : _options)
    XBU::verbose(" " + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).run(), vm);
    po::notify(vm); // Can throw
  }
  catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    printHelp();
    throw std::errc::operation_canceled;
  }

  if(m_help) {
    printHelp();
    return;
  }

  XBU::sudo_or_throw("ERROR: root privileges required.");
 
  try {
      spiCommand(vm);
  }
  catch (const std::runtime_error& e) {
      // Catch only the exceptions that we have generated earlier
      std::cerr << boost::format("ERROR: %s\n") % e.what() << std::endl;
      throw std::runtime_error("Program execution failed - Flash type spi.");
  }
  catch (...) {
      printHelp();
      throw std::runtime_error("Program execution failed - Flash type spi.");
  }

  std::cout << "****************************************************\n";
  std::cout << "Cold reboot machine to load the new image on device.\n";
  std::cout << "****************************************************\n";
  return;
}
