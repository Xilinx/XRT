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
#include "OO_Dump_Qspips.h"
#include "tools/common/XBUtilitiesCore.h"

#include "core/pcie/tools/xbflash.qspi/firmware_image.h"
#include "core/pcie/tools/xbflash.qspi/pcidev.h"
#include "core/pcie/tools/xbflash.qspi/xqspips.h"

namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

namespace po = boost::program_options;

// System - Include Files
#include <iostream>

namespace {

int
qspips_readback(po::variables_map& vm)
{
    size_t offset = 0, len = FLASH_SIZE;
    std::string bdf;
    std::string soffset;
    std::string slength;
    std::string flash_type;
    std::string output;
    std::string sBar, sBarOffset;
    size_t baroff = INVALID_OFFSET;
    int bar = 0;

    XBU::sudo_or_throw_err();

    try {
        if (vm.count("device")) {
            bdf = vm["device"].as<std::string>();
        }
        else {
            std::cerr << "\nERROR: Device not specified. Please specify a single device using --device option\n";
            throw std::errc::invalid_argument;
        }

        if (vm.count("output")) {
            output = vm["output"].as<std::string>();
        }
        else {
            std::cerr << "\nERROR: output not specified. Please specify the output file path using --output option\n";
            throw std::errc::invalid_argument;
        }
    }
    catch (...) {
        throw std::errc::invalid_argument;
    }

    try {
        //Optional arguments
        if (vm.count("flash-part"))
            flash_type = vm["flash-part"].as<std::string>();

        if (vm.count("offset")) {
            soffset = vm["offset"].as<std::string>();
            if (!soffset.empty()) {
                std::stringstream sstream(soffset);
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

    //Optional arguments
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

    if (bdf.empty() || output.empty())
        throw std::errc::invalid_argument;

    std::cout << "Read out flash"
        << boost::format("[0x%x, 0x%x] on device %s to %s\n") % offset % (offset + len) % bdf % output;

    pcidev::pci_device dev(bdf, bar, baroff, flash_type);
    XQSPIPS_Flasher qspips(&dev);

    return qspips.xclReadBack(output, offset, len);
}
} //end namespace 

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Dump_Qspips::OO_Dump_Qspips( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Flash type - qspips")
    , m_help(false)
{
  m_optionsDescription.add_options()
      ("device,d", po::value<std::string>(), "Bus:Device.Function\" (e.g., 0000:d8:00.0) device of interest.\n")
      ("offset,a", po::value<std::string>(), "offset on flash to start, default is 0.\n")
      ("length,l", po::value<std::string>(), "length-to-read, default is 128MB.\n")
      ("flash-part,p", po::value<std::string>(), "qspips-flash-part, default is qspi_ps_x2_single.\n")
      ("bar,b", po::value<std::string>(), "BAR-index-for-QSPIPS, default is 0.\n")
      ("bar-offset,s", po::value<std::string>(), "BAR-offset-for-QSPIPS, default is 0x40000.\n")
      ("output,o", po::value<std::string>(), "output file to save read contents.\n")
      ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
OO_Dump_Qspips::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand dump - option: Flash type - qspips");

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
      if (qspips_readback(vm)) {
          throw std::errc::operation_canceled;
      }
  }
  catch (...) {
      std::cerr << "ERROR: Dump execution - Flash type qspips " << std::endl;
      printHelp();
      throw std::errc::operation_canceled;
  }

  std::cout << "****************************************************\n";
  std::cout << "Successfully dumped the output to the given file.\n";
  std::cout << "****************************************************\n";
  return;
}
