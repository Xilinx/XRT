/**
 * Copyright (C) 2020 Licensed under the Apache License, Version
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
#include "OO_NIFD.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/common/device.h"
#include "core/common/system.h"
#include "core/common/error.h"
#include "core/common/utils.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <vector>
#include <fstream>
#include <fcntl.h>


// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {
static void
device_status(const std::string& device) 
{
  auto dev = xrt_core::get_mgmtpf_device(xrt_core::utils::bdf2index(device));
  xrt_core::scope_value_guard<int, std::function<void()>> fd { 0, nullptr };
  try {
      fd = dev->file_open("nifd_pri", O_RDWR); 
  } catch (const std::exception& e) {
      xrt_core::send_exception_message(e.what(), "XBMGMT");
  }

  // const int NIFD_CHECK_STATUS = 8;
  unsigned int statusReg = 0;
  int result = 0;//dev->ioctl(fd.get(), NIFD_CHECK_STATUS, &statusReg);
  if (result != 0)
    throw xrt_core::error("Could not read status register");
  else
    std::cout << boost::format("Current NIFD status: 0x%x\n") % statusReg;
}

static void
readback(const std::string& device, const std::string& file)
{
  std::ifstream fin(file.c_str()) ;
  if (!fin)
    throw xrt_core::error(boost::str(boost::format("Could not open %s for reading") % file));

  std::vector<unsigned int> hardwareFramesAndOffsets;
  while (!fin.eof())
  {
    unsigned int nextNum = 0 ;
    fin >> std::ws >> nextNum >> std::ws ;
    hardwareFramesAndOffsets.push_back(nextNum);
  }
  fin.close();

  auto dev = xrt_core::get_mgmtpf_device(xrt_core::utils::bdf2index(device));
  xrt_core::scope_value_guard<int, std::function<void()>> fd { 0, nullptr };
  try {
      fd = dev->file_open("nifd_pri", O_RDWR); 
  } catch (const std::exception& e) {
      xrt_core::send_exception_message(e.what(), "XBMGMT");
  }
  
  unsigned int numBits = static_cast<unsigned int>(hardwareFramesAndOffsets.size() / 2);
  unsigned int resultWords = numBits % 32 ? numBits/32 + 1 : numBits/32 ;
  std::vector<uint64_t> packet(1 + numBits * 2 + resultWords);
  packet.push_back(numBits) ;
  for (unsigned int i = 0 ; i < hardwareFramesAndOffsets.size() ; ++i)
    packet.push_back(hardwareFramesAndOffsets[i]);

  // const int NIFD_READBACK_VARIABLE = 3 ;
  // const int NIFD_SWITCH_ICAP_TO_NIFD = 4 ;
  // const int NIFD_SWITCH_ICAP_TO_PR = 5 ;
  int result = 0 ;
  // result = dev->ioctl(fd.get(), NIFD_SWITCH_ICAP_TO_NIFD);
  if (result != 0)
    throw xrt_core::error("Could not switch ICAP to NIFD control");

  // result = dev->ioctl(fd.get(), NIFD_READBACK_VARIABLE, packet.data()) ;
  // result |= dev->ioctl(fd.get(), NIFD_SWITCH_ICAP_TO_PR);
  if (result != 0)
    throw xrt_core::error("Could not readback variable!");
  
  std::cout << "Value read: " ;
  for (unsigned int i = 0 ; i < resultWords ; ++i)
    std::cout << boost::format("0x%x ") % (packet[1 + numBits*2 + i]);
}

}
//end anonymous namespace


// ----- C L A S S   M E T H O D S -------------------------------------------

OO_NIFD::OO_NIFD( const std::string &_longName)
    : OptionOptions(_longName, "<add description>")
    , m_device("")
    , m_help(false)
    , status(false)
    , readback_file("")
{
  setExtendedHelp("<add description>");

  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("status", boost::program_options::bool_switch(&status), "<add description>")
    ("read-back", boost::program_options::value<decltype(readback_file)>(&readback_file), "<add description>")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("name", 1 /* max_count */).
    add("frequency", 1 /* max_count */)
  ;
}

void
OO_NIFD::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand option: nifd");
  XBU::verbose("Option(s):");
  for (auto & aString : _options) 
    XBU::verbose(std::string(" ") + aString);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp();
    throw; // Re-throw exception
  }

  //DRC checks
  if(m_help || m_device.empty()) {
    printHelp();
    return;
  }

  //Option: status
  if(status)
    device_status(m_device);

  //Option: readback
  if(!readback_file.empty())
    readback(m_device, readback_file);
}
