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
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

namespace {

int 
qspips_flash(po::variables_map& vm) {
    std::string bdf;
    std::string flash_type;
    std::string bin_file;
    std::vector<std::string> bin_files;
    std::string soffset;
    size_t offset = 0;
    bool force = false;
    std::string sBar, sBarOffset;
    size_t baroff = INVALID_OFFSET;
    int bar = 0;   

    XBU::sudo_or_throw_err();
     
    //mandatory command line args
    try {
        bdf = vm["device"].as<std::string>();
        bin_files = vm["image"].as<std::vector<std::string>>();
    }
    catch (...) {
        throw std::errc::invalid_argument;
    }

    //optional command line args
    try {
        if (vm.count("flash-part"))
            flash_type = vm["flash-part"].as<std::string>();
        if (vm.count("offset")) {
            soffset = vm["offset"].as<std::string>();
            if (!soffset.empty()) {
                std::stringstream sstream(soffset);
                sstream >> offset;
            }
        }
        if (vm.count("force"))
            force = true;
    }
    catch (...) {
    }

    //optionals command line args
    try {
        if (vm.count("bar")) {
            sBar = vm["bar"].as<std::string>();
            if (!sBar.empty())
                bar = std::stoi(sBar);
        }
        if (vm.count("bar-offset")) {
            sBarOffset = vm["bar-offset"].as<std::string>();
            if (!sBarOffset.empty()) {
                std::stringstream sstream(sBarOffset);
                sstream >> baroff;
            }
        }
    }
    catch (...) {
    }

    if (bdf.empty()) {
        std::cout << "Error: Please provide mgmt BDF\n";
        throw std::errc::invalid_argument;
    }

    if (!bin_files.size()) {
        std::cout << "Error: Please provide proper BIN file.\n";
        throw std::errc::invalid_argument;
    }

    firmwareImage bin(bin_files[0].c_str());
    if (bin.fail()) {
        std::cout << "Error: Please provide proper BIN file.\n";
        throw std::errc::invalid_argument;
    }
    std::cout << "About to program flash on device "
        << boost::format(" %s at offset 0x%x\n") % bdf % offset;

    if (!force && !XBU::can_proceed())
        throw std::errc::operation_canceled;

    pcidev::pci_device dev(bdf, bar, baroff, flash_type);
    XQSPIPS_Flasher qspips(&dev);

    return qspips.xclUpgradeFirmware(bin, offset);
}

int 
qspips_erase(po::variables_map& vm) {
    std::string bdf;
    std::string flash_type;
    std::string soffset;
    std::string slength;
    std::string output;
    size_t offset = 0, len = GOLDEN_BASE;
    bool force = false;
    std::string sBar, sBarOffset;
    size_t baroff = INVALID_OFFSET;
    int bar = 0;    

    XBU::sudo_or_throw_err();
    //mandatory command options
    try {
        bdf = vm["device"].as<std::string>();
    }
    catch (...) {
        throw std::errc::invalid_argument;
    }

    //optionals command options
    try {
        if (vm.count("flash-part"))
            flash_type = vm["flash-part"].as<std::string>();
        if (vm.count("offset")) {
            soffset = vm["offset"].as<std::string>();
            if (!soffset.empty()) {
                std::stringstream sstream(soffset);
                if (offset != INVALID_OFFSET)
                    sstream >> offset;
            }
        }
        if (vm.count("length")) {
            slength = vm["length"].as<std::string>();
            if (!slength.empty()) {
                std::stringstream sstream(slength);
                sstream >> len;
            }
        }
    }
    catch (...) {
    }

    //optionals command line args
    try {
        if (vm.count("bar")) {
            sBar = vm["bar"].as<std::string>();
            if (!sBar.empty())
                bar = std::stoi(sBar);
        }

        if (vm.count("bar-offset")) {
            sBarOffset = vm["bar-offset"].as<std::string>();
            if (!sBarOffset.empty()) {
                std::stringstream sstream(sBarOffset);
                sstream >> baroff;
            }
        }
    }
    catch (...) {
    }

    if (bdf.empty()) {
        std::cout << "Error: Please provide mgmt BDF\n";
        throw std::errc::invalid_argument;
    }

    std::cout << "About to erase flash"
        << boost::format(" [0x%x, 0x%x] on device %s\n") % offset % (offset + len) % bdf;

    if (offset + len > GOLDEN_BASE)
        std::cout << "\nThis might erase golden image if there is !!\n" << std::endl;

    if (!force && !XBU::can_proceed())
        throw std::errc::operation_canceled;

    pcidev::pci_device dev(bdf, bar, baroff, flash_type);
    XQSPIPS_Flasher qspips(&dev);

    return qspips.xclErase(offset, len);
}

int
qspipsCommand(po::variables_map& vm)
{
    if (!vm.count("device")) {
        std::cerr << "\nERROR: Device not specified. Please specify a single device using --device option\n";
        throw std::errc::invalid_argument;
    }

    if (vm.count("erase")) {
        return qspips_erase(vm);
    }
    else if (vm.count("image")) {
        return qspips_flash(vm);
    }
    std::cout << "\nERROR: Missing program operation. No action taken.\n\n";
    return 1;
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

  XBU::sudo_or_throw_err();
  try {
      if (qspipsCommand(vm)) {
          throw std::errc::operation_canceled;
      }
  }
  catch (...) {
      std::cerr << "ERROR: Program execution - Flash type qspips " << std::endl;
      printHelp();
      throw std::errc::operation_canceled;
  }

  std::cout << "****************************************************\n";
  std::cout << "Cold reboot machine to load the new image on device.\n";
  std::cout << "****************************************************\n";
  return;
}
