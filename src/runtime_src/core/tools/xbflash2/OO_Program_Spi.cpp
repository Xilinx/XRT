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
#include "XBFUtilities.h"

#include "core/pcie/tools/xbflash.qspi/firmware_image.h"
#include "core/pcie/tools/xbflash.qspi/pcidev.h"
#include "core/pcie/tools/xbflash.qspi/xspi.h"

namespace XBFU = XBFUtilities;
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

int
reset(po::variables_map vm, int bar, size_t baroff) {
    std::string bdf;
    bool force = false;
    bool dualflash = false;
    XBFU::sudo_or_throw();
    //mandatory command line args
    try {
        bdf = vm["device"].as<std::string>();
    }
    catch (...) {
        return -EINVAL;
    }

    //optional command line args
    try {
        if (vm.count("force"))
            force = true;
        if (vm.count("dual-flash"))
            dualflash = true;
    }
    catch (...) {
    }

    std::cout << "About to revert to golden image for device " << bdf << std::endl;

    if (!force && !XBFU::can_proceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff);
    XSPI_Flasher xspi(&dev, dualflash);
    return xspi.revertToMFG();
}

int
flash(po::variables_map vm, int bar, size_t baroff) {
    std::string bdf;
    std::vector <std::string> primary_file;
    int ret = 0;
    bool force = false;
    bool dual_flash = false;

    XBFU::sudo_or_throw();

    //mandatory command line args
    try {
        bdf = vm["device"].as<std::string>();
        primary_file = vm["image"].as<std::vector<std::string>>();
        if (primary_file.size() == 2)
            dual_flash = true;
    }
    catch (...) {
        return -EINVAL;
    }

    if (vm.count("force"))
        force = true;

    std::cout
        << "About to flash below MCS bitstream onto device "
        << bdf << ":" << std::endl;

    if (!force && !XBFU::can_proceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff);
    XSPI_Flasher xspi(&dev, dual_flash);

    if (!dual_flash) {
        firmwareImage pri(primary_file[0].c_str());
        if (pri.fail())
            return -EINVAL;
        ret = xspi.xclUpgradeFirmware1(pri);
    }
    else {
        firmwareImage pri(primary_file[0].c_str());
        firmwareImage sec(primary_file[1].c_str());
        if (pri.fail() || sec.fail())
            return -EINVAL;
        ret = xspi.xclUpgradeFirmware2(pri, sec);
    }
    return ret;
}

int
spiCommand(po::variables_map vm) {
    std::string sBar, sBarOffset;
    size_t baroff = INVALID_OFFSET;
    int bar = 0;
    int err = -EINVAL;

    //optional command line args
    try {
        sBar = vm["bar"].as<std::string>();
        if (!sBar.empty())
            bar = std::stoi(sBar);
        sBarOffset = vm["bar-offset"].as<std::string>();
        if (!sBarOffset.empty()) {
            std::stringstream sstream(sBarOffset);
            sstream >> baroff;
        }
    }
    catch (...) {
    }

    if (vm.count("revert-to-golden")) {
        err = reset(vm, bar, baroff);
    }
    else if (vm.count("image")) {
        err = flash(vm, bar, baroff);
    }

    return err;
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
    XBU::verbose(std::string(" ") + aString);

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
    throw std::errc::operation_canceled;
  }

  XBFU::sudo_or_throw();
 
  if (spiCommand(vm) == -EINVAL)
  {
      std::cerr << "ERROR: Program execution - Flash type spi " << std::endl;
      printHelp();
      return;
  }
  std::cout << "****************************************************\n";
  std::cout << "Successfully flashed the image on device.\n ";
  std::cout << "Cold reboot machine to load the new image on device.\n ";
  std::cout << "****************************************************\n";
  return;
}
