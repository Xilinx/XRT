/**
 * Copyright (C) 2019 Xilinx, Inc
 * Author: Jason Villarreal
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

#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <getopt.h>
#include <climits>

#include "xbmgmt.h"
#include "core/pcie/linux/scan.h"
#include "core/common/utils.h"

const char *subCmdNifdDesc = "Access the NIFD debug IP to readback frames and offsets";
const char *subCmdNifdUsage = "--status [--card bdf]\n--readback <frame/offset file> [--card bdf]";

#if defined (_WIN32)
// Accessing NIFD unsupported on Windows
static int status(unsigned int index)
{
  return 0 ;
}

static int readback(const std::string& inputFile, unsigned int index)
{
  return 0 ;
}
#else

static int status(unsigned int index)
{
  std::shared_ptr<pcidev::pci_device> dev = pcidev::get_dev(index, false);
  int fd = dev->open("nifd_pri", O_RDWR);
  if (fd == -1)
  {
    std::cout << "NIFD IP not available on selected device" << std::endl;
    return -errno;
  }
	  
  const int NIFD_CHECK_STATUS = 8;
  unsigned int status = 0;
  int result = dev->ioctl(fd, NIFD_CHECK_STATUS, &status);
  if (result != 0)
  {
    std::cout << "ERROR: Could not read status register" << std::endl;
  }
  else
  {
    xrt_core::ios_flags_restore format(std::cout);
    std::cout << "Current NIFD status: 0x" << std::hex << status << std::endl;
  }
  dev->close(fd);
  return 0;
}

static int readback(const std::string& inputFile, unsigned int index)
{
  std::ifstream fin(inputFile.c_str()) ;
  if (!fin)
  {
    std::cout << "Could not open " << inputFile << " for reading" << std::endl ;
    return 0 ;
  }

  std::vector<unsigned int> hardwareFramesAndOffsets;
  while (!fin.eof())
  {
    unsigned int nextNum = 0 ;
    fin >> std::ws >> nextNum >> std::ws ;
    hardwareFramesAndOffsets.push_back(nextNum) ;
  }
  fin.close() ;

  std::shared_ptr<pcidev::pci_device> dev = pcidev::get_dev(index, false);
  int fd = dev->open("nifd_pri", O_RDWR);
  if (fd == -1)
  {
    std::cout << "NIFD IP not available on selected device" << std::endl;
    return -errno;
  }
	  
  unsigned int numBits = hardwareFramesAndOffsets.size() / 2 ;
  unsigned int resultWords = numBits % 32 ? numBits/32 + 1 : numBits/32 ;
  unsigned int packet[1 + numBits*2 + resultWords] = {0} ;
  packet[0] = numBits ;
  for (unsigned int i = 0 ; i < hardwareFramesAndOffsets.size() ; ++i)
  {
    packet[i+1] = hardwareFramesAndOffsets[i] ;
  }

  const int NIFD_READBACK_VARIABLE = 3 ;
  const int NIFD_SWITCH_ICAP_TO_NIFD = 4 ;
  const int NIFD_SWITCH_ICAP_TO_PR = 5 ;
  int result = 0 ;
  result = dev->ioctl(fd, NIFD_SWITCH_ICAP_TO_NIFD);
  if (result != 0)
  {
    std::cout << "ERROR: Could not switch ICAP to NIFD control" << std::endl ;
    dev->close(fd) ;
    return 0 ;
  }
  result = dev->ioctl(fd, NIFD_READBACK_VARIABLE, packet) ;
  result |= dev->ioctl(fd, NIFD_SWITCH_ICAP_TO_PR);
  if (result != 0)
  {
    std::cout << "ERROR: Could not readback variable!" << std::endl ;
    dev->close(fd) ;
    return 0 ;
  }

  xrt_core::ios_flags_restore format(std::cout);
  std::cout << "Value read: " ;
  for (unsigned int i = 0 ; i < resultWords ; ++i)
  {
    std::cout << std::hex << "0x" << packet[1 + numBits*2 + i] << " " ;
  }
  std::cout << std::endl ;

  dev->close(fd) ;
  return 0 ;
}

#endif

int nifdHandler(int argc, char* argv[])
{
  sudoOrDie() ;
  
  if (argc < 2)
    return -EINVAL ;

  unsigned int index = 0 ; // Default to first device
  bool status = false ;
  bool readback = false ;
  std::string inputFile ;

  const option opts[] = {
    { "status",   no_argument,       nullptr, '0' },
    { "readback", required_argument, nullptr, '1' },
    { "card",     required_argument, nullptr, '2' },
    { nullptr, 0, nullptr, 0 },
  };

  while (true)
  {
    const auto opt = getopt_long(argc, argv, "", opts, nullptr) ;
    if (opt == -1)
      break ;

    switch(opt)
    {
    case '0':
      status = true ;
      break ;
    case '1':
      readback = true ;
      inputFile = std::string(optarg);
      break ;
    case '2':
      index = bdf2index(optarg);
      if (index == UINT_MAX)
	return -ENOENT;
      break ;
    default:
      return -EINVAL ;
    }
  }

  if (status)
    return ::status(index) ;
  if (readback)
    return ::readback(inputFile, index);

  return -EINVAL ;
}
