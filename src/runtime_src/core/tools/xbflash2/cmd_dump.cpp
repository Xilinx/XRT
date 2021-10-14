/**
 * Copyright (C) 2021 Xilinx, Inc
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

#include "xbflash2.h"
#include <string>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <map>
#include <functional>
#include <getopt.h>

// For backward compatibility
const char *subCmdDumpDesc = "Reads the image(s) for a given device for a given length and outputs the same to given file. It is applicable for only QSPIPS flash.";
const char *subCmdDumpUsage =
    "--qspips [--device mgmt-bdf] --output <arg> [--bar arg] [--bar-offset arg] [--offset arg] [--length arg] [--flash-part arg] \n"
    "\nOPTIONS:\n"
    "\t--device: The \"Bus:Device.Function\" (e.g., 0000:d8:00.0) device of interest.\n"
    "\t--output: output file to save read contents\n"
    "\t--qspips: Use it for QSPIPS flash\n"
    "\t--offset: offset on flash to start, default is 0\n"
    "\t--flash-part: qspips-flash-part, default is qspi_ps_x2_single\n"
    "\t--bar: BAR-index-for-QSPIPS, default is 0\n"
    "\t--length: length-to-read, default is 128MB\n"
    "\t--bar-offset: BAR-index-for-QSPIPS, default is 0x40000\n";

static int qspips_readback(po::variables_map vm, int bar, size_t baroff)
{
    size_t offset = 0, len = FLASH_SIZE;
    std::string bdf;
    std::string soffset;
    std::string slength;
    std::string flash_type;
    std::string output;

    sudoOrDie();

    try {
      bdf = vm["device"].as<std::string>();
      output = vm["output"].as<std::string>();
    } catch (...) {
      return -EINVAL;
    }

    try {
      flash_type = vm["flash-part"].as<std::string>();
      soffset = vm["offset"].as<std::string>();
      if (!soffset.empty()) {
        std::stringstream sstream(soffset);
        sstream >> offset;
      }
      slength = vm["length"].as<std::string>();
      if (!slength.empty()) {
        std::stringstream sstream(slength);
        sstream >> len;
      }
    } catch (...) {
    }

    if (bdf.empty() || output.empty())
      return -EINVAL;

    std::cout << "Read out flash"
        << " [0x" << std::hex << offset << ",0x" << offset+len
        << "] on device " << bdf << " to " << output << std::dec << std::endl;

    pcidev::pci_device dev(bdf, bar, baroff, flash_type);
    XQSPIPS_Flasher qspips(&dev);

    return qspips.xclReadBack(output, offset, len);
}

static int qspipsCommand(po::variables_map vm, int argc, char *argv[])
{
    std::string sBar, sBarOffset;
    size_t baroff = INVALID_OFFSET;
    int bar = 0;

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

    return qspips_readback(vm, bar, baroff);
}

static const std::map<std::string, std::function<int(po::variables_map, int, char **)>> optList = {
    { "--qspips", qspipsCommand },
};

int dumpHandler(po::variables_map vm, int argc, char *argv[])
{
    if (argc < 2)
        return -EINVAL;

    sudoOrDie();

    std::string subcmd(argv[1]);

    auto cmd = optList.find(subcmd);
    if (cmd == optList.end())
        return -EINVAL;

    argc--;
    argv++;
    return cmd->second(vm, argc, argv);
}
