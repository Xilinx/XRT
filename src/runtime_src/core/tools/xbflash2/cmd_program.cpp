/**
 * Copyright (C) 2022 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include "xbflash2.h"

#ifdef _WIN32
# pragma warning( disable : 4267)
#endif

// For backward compatibility
const char *subCmdProgramDesc = "Updates the image(s) for a given device";
const char *subCmdProgramUsage =
    "--flash qspips --device <mgmt-bdf> --image <input-arg.BIN> [--offset <arg>] [--flash-part <arg>] [--bar <arg>] [--bar-offset <arg>]\n"
    "--flash qspips --device <mgmt-bdf> --erase [--offset <arg>] [--flash-part <arg>] [--bar <arg>] [--bar-offset <arg>]\n"
    "--flash spi --device <mgmt-bdf> --image primary_mcs [--image arg] [--bar <arg>] [--bar-offset <arg>]\n"
    "--flash spi --device <mgmt-bdf> --revert-to-golden [--dual-flash] [--bar <arg>] [--bar-offset <arg>] [--force, yes for prompt]\n"
    "\nOPTIONS:\n"
    "\t--device: The \"Bus:Device.Function\" (e.g., 0000:d8:00.0) device of interest.\n"
    "\t--flash: spi    - Use it for SPI flash\n"
    "\t         qspips - Use it for QSPIPS flash\n"
    "\t--image: Specifies MCS or BOOT.BIN image path to update the persistent device\n"
    "\t--revert-to-golden: Resets the FPGA PROM back to the factory image.\n"
    "\t--dual-flash: Specifies if the card is dual flash supported\n"
    "\t--offset: offset on flash to start, default is 0\n"
    "\t--flash-part: qspips-flash-type, default is qspi_ps_x2_single\n"
    "\t--bar: BAR-index, default is 0\n"
    "\t--bar-offset: BAR-offset-for-QSPIPS, default is 0x40000\n"
    "\t--force: When possible, force an operation\n";

static int reset(po::variables_map vm, int bar, size_t baroff)
{
    std::string bdf;
    bool force = false;
    bool dualflash = false;

    sudoOrDie();

    //mandatory command line args
    try {
      bdf = vm["device"].as<std::string>();
    } catch (...) {
      return -EINVAL;
    }

    //optional command line args
    try {
      if (vm.count("force"))
        force = true;
      if (vm.count("dual-flash"))
        dualflash = true;
    } catch (...) {
    }

    std::cout << "About to revert to golden image for device " << bdf << std::endl;

    if (!force && !canProceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff);
    XSPI_Flasher xspi(&dev, dualflash);
    return xspi.revertToMFG();
}

static int flash(po::variables_map vm, int bar, size_t baroff)
{
    std::string bdf;
    std::vector <std::string> primary_file;
    int ret = 0;
    bool force = false;
    bool dual_flash = false;

    sudoOrDie();

    //mandatory command line args
    try {
      bdf = vm["device"].as<std::string>();
      primary_file = vm["image"].as<std::vector<std::string>>();
      if (primary_file.size() == 2)
        dual_flash = true;
    } catch (...) {
      return -EINVAL;
    }

    if (vm.count("force"))
      force = true;

    std::cout
        << "About to flash below MCS bitstream onto device "
        << bdf << ":" << std::endl;

    if (!force && !canProceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff);
    XSPI_Flasher xspi(&dev, dual_flash);

    if (!dual_flash) {
        firmwareImage pri(primary_file[0].c_str());
        if (pri.fail())
            return -EINVAL;
        ret = xspi.xclUpgradeFirmware1(pri);
    } else {
        firmwareImage pri(primary_file[0].c_str());
        firmwareImage sec(primary_file[1].c_str());
        if (pri.fail() || sec.fail())
            return -EINVAL;
        ret = xspi.xclUpgradeFirmware2(pri, sec);
    }

    return ret;
}

static int spiCommand(po::variables_map vm)
{
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
    } catch (...) {
    }

    if (vm.count("revert-to-golden")) {
      err = reset(vm, bar, baroff);
    } else if (vm.count("image")) {
      err = flash(vm, bar, baroff);
    }

    return err;
}

static int qspips_flash(po::variables_map vm, int bar, size_t baroff)
{
    std::string bdf;
    std::string flash_type;
    std::string bin_file;
    std::vector<std::string> bin_files;
    std::string soffset;
    size_t offset = 0;
    bool force = false;

    sudoOrDie();
 
    //mandatory command line args
    try {
      bdf = vm["device"].as<std::string>();
      bin_files = vm["image"].as<std::vector<std::string>>();
    } catch (...) {
      return -EINVAL;
    }

    //optional command line args
    try {
      flash_type = vm["flash-part"].as<std::string>();
      soffset = vm["offset"].as<std::string>();
      if (!soffset.empty()) {
        std::stringstream sstream(soffset);
        sstream >> offset;
      }
      if (vm.count("force"))
        force = true;
    } catch (...) {
    }

    if (bdf.empty()) {
      std::cout << "Error: Please provide mgmt BDF\n";
      return -EINVAL;
    }

    if (!bin_files.size()) {
      std::cout << "Error: Please provide proper BIN file.\n";
      return -EINVAL;
    }

    firmwareImage bin(bin_files[0].c_str());
    if (bin.fail()) {
      std::cout << "Error: Please provide proper BIN file.\n";
      return -EINVAL;
    }
    std::cout << "About to program flash on device "
        << boost::format(" %s at offset 0x%x\n") % bdf % offset;

    if (!force && !canProceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff, flash_type);
    XQSPIPS_Flasher qspips(&dev);

    return qspips.xclUpgradeFirmware(bin, offset);
}

static int qspips_erase(po::variables_map vm, int bar, size_t baroff)
{
    std::string bdf;
    std::string flash_type;
    std::string soffset;
    std::string slength;
    std::string output;
    size_t offset = 0, len = GOLDEN_BASE;
    bool force = false;

    sudoOrDie();

    //mandatory command options
    try {
      bdf = vm["device"].as<std::string>();
    } catch (...) {
      return -EINVAL;
    }

    //optionals command options
    try {
      flash_type = vm["flash-part"].as<std::string>();
      soffset = vm["offset"].as<std::string>();
      if (!soffset.empty()) {
        std::stringstream sstream(soffset);
        if (offset != INVALID_OFFSET)
          sstream >> offset;
      }
      slength = vm["length"].as<std::string>();
      if (!slength.empty()) {
        std::stringstream sstream(slength);
        sstream >> len;
      }
    } catch (...) {
    }

    if (bdf.empty()) {
      std::cout << "Error: Please provide mgmt BDF\n";
      return -EINVAL;
    }

    std::cout << "About to erase flash"
        << boost::format(" [0x%x, 0x%x] on device %s\n") % offset % (offset+len) % bdf;

    if (offset + len > GOLDEN_BASE)
        std::cout << "\nThis might erase golden image if there is !!\n" << std::endl;

    if (!force && !canProceed())
        return -ECANCELED;

    pcidev::pci_device dev(bdf, bar, baroff, flash_type);
    XQSPIPS_Flasher qspips(&dev);

    return qspips.xclErase(offset, len);
}

static int qspipsCommand(po::variables_map vm)
{
    std::string sBar, sBarOffset;
    size_t baroff = INVALID_OFFSET;
    int bar = 0;
    int err = 0;

    //optionals command line args
    try {
      sBar = vm["bar"].as<std::string>();
      if (!sBar.empty())
        bar = std::stoi(sBar);
      sBarOffset = vm["bar-offset"].as<std::string>();
      if (!sBarOffset.empty()) {
        std::stringstream sstream(sBarOffset);
        sstream >> baroff;
      }
    } catch (...) {
    }
 
    if (vm.count("erase")) {
       err = qspips_erase(vm, bar, baroff);
    } else if (vm.count("image")) {
       err = qspips_flash(vm, bar, baroff);
    }

    return err;
}

static const std::map<std::string, std::function<int(po::variables_map)>> optList = {
    { "spi", spiCommand },
    { "qspips", qspipsCommand },
};

int programHandler(po::variables_map vm)
{
    std::string subcmd;

    sudoOrDie();

    try {
     subcmd = vm["flash"].as<std::string>();
    } catch (...) {
      return -EINVAL;
    }

    auto cmd = optList.find(subcmd);
    if (cmd == optList.end())
        return -EINVAL;

    return cmd->second(vm);
}
