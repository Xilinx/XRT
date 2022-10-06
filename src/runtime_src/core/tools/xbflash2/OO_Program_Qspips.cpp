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
#include "OO_Program_Qspips.h"
#include "tools/common/XBUtilitiesCore.h"

#include "core/pcie/tools/xbflash.qspi/firmware_image.h"
#include "core/pcie/tools/xbflash.qspi/pcidev.h"
#include "core/pcie/tools/xbflash.qspi/xqspips.h"

namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

namespace {

void
qspipsCommand(po::variables_map& vm)
{
    //root privileges required.
    XBU::sudo_or_throw_err();

    //mandatory command line args
    std::string bdf = vm.count("device") ? vm["device"].as<std::string>() : "";
    if (bdf.empty())
        throw std::runtime_error("Device not specified. Please specify a single device using --device option");

    //optional command line args
    std::string flash_type = vm.count("flash-part") ? vm["flash-part"].as<std::string>() : "";
    std::string soffset = vm.count("offset") ? vm["offset"].as<std::string>() : "";
    size_t offset = !soffset.empty() ? std::stoul(soffset, nullptr, 0) : 0;
    bool force = vm.count("force") ? true : false;
    std::string sBar = vm.count("bar") ? vm["bar"].as<std::string>() : "";
    int bar = !sBar.empty() ? std::stoi(sBar, nullptr, 0) : 0;
    std::string sBarOffset = vm.count("bar-offset") ? vm["bar-offset"].as<std::string>() : "";
    size_t baroff = !sBarOffset.empty() ? std::stoul(sBarOffset, nullptr, 0) : INVALID_OFFSET;

    if (vm.count("erase")) { //qspips erase
        std::string slength = vm.count("length") ? vm["length"].as<std::string>() : "";
        size_t len = !slength.empty()  ? std::stoi(slength, nullptr, 0) : GOLDEN_BASE;

        std::cout << "About to erase flash"
            << boost::format(" [0x%x, 0x%x] on device %s\n") % offset % (offset + len) % bdf;

        if (offset + len > GOLDEN_BASE)
            std::cout << "\nProvided length from the offset is crossing golden image boundary, This might erase golden image.\n" << std::endl;

        if (!force && !XBU::can_proceed())
            return;

        pcidev::pci_device dev(bdf, bar, baroff, flash_type);
        XQSPIPS_Flasher qspips(&dev);

        if (qspips.xclErase(offset, len))
            throw std::runtime_error("qspips erase failed.");
        return;
    }
        
    if (vm.count("image")) {// qspips flash 
        std::vector<std::string> bin_files = vm["image"].as<std::vector<std::string>>();

        if (!bin_files.size())
            throw std::runtime_error("Please provide proper BIN file.");

        firmwareImage bin(bin_files[0].c_str());
        if (bin.fail())
            throw std::runtime_error("Please provide proper BIN file.");

        std::cout << "Preparing to program flash on device: "
            << boost::format(" %s at offset 0x%x\n") % bdf % offset;

        if (!force && !XBU::can_proceed())
            return;

        pcidev::pci_device dev(bdf, bar, baroff, flash_type);
        XQSPIPS_Flasher qspips(&dev);

        if (qspips.xclUpgradeFirmware(bin, offset))
            throw std::runtime_error("qspips flash failed.");
        return;
    }

    throw std::runtime_error("Missing program operation.No action taken.");
}
} //end namespace 

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Program_Qspips::OO_Program_Qspips( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Flash type - qspips")
    , m_help(false)
{
  m_optionsDescription.add_options()
      ("device,d", po::value<std::string>(), "The \"Bus:Device.Function\" (e.g., 0000:d8:00.0) device of interest.\n")
      ("offset,a", po::value<std::string>(), "Offset on flash to start, default is 0.\n")
      ("flash-part,p", po::value<std::string>(), "qspips-flash-type, default is qspi_ps_x2_single.\n")
      ("bar,b", po::value<std::string>(), "BAR-index, default is 0.\n")
      ("bar-offset,s", po::value<std::string>(), "BAR-offset-for-QSPIPS, default is 0x40000.\n")
      ("length,l", po::value<std::string>(), "length-to-erase, default is 96MB.\n")
      ("image,i", po::value<std::vector<std::string>>(), "Specifies MCS or BOOT.BIN image path to update the persistent device.\n")
      ("erase,e", "Erase flash on the device.\n")
      ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
OO_Program_Qspips::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand option: Flash type - qspips");

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

  XBU::sudo_or_throw_err();
  try {
      qspipsCommand(vm);      
  }
  catch (const std::runtime_error& e) {
      // Catch only the exceptions that we have generated earlier
      std::cerr << boost::format("ERROR: %s\n") % e.what() << std::endl;
      throw std::runtime_error("Program execution failed - Flash type qspips.");
  }
  catch (...) {
      printHelp();
      throw std::runtime_error("Program execution failed - Flash type qspips.");
  }

  std::cout << "****************************************************\n";
  std::cout << "Cold reboot machine to load the new image on device.\n";
  std::cout << "****************************************************\n";
  return;
}
