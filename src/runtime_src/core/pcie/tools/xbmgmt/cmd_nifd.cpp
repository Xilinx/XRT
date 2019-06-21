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

#if !defined(_WIN32)
// Linux specific headers
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "xbmgmt.h"
#include "core/pcie/linux/scan.h"

const char *subCmdNifdDesc = "Access the NIFD debug IP to readback frames and offsets";
const char *subCmdNifdUsage = "--status\n--readback <frame/offset file>";

#if defined (_WIN32)
// Accessing NIFD unsupported on Windows
static int status()
{
  return 0 ;
}

static int readback(const std::string& inputFile)
{
  return 0 ;
}
#else

std::string NIFDDevFile()
{
  std::shared_ptr<pcidev::pci_device> dev = pcidev::get_dev(0, false);
  const int instance = (dev->domain << 16) + (dev->bus << 8) + (dev->dev << 3) + (dev->func) ;

  std::string fileName = "/dev/nifd_pri.m" ;
  fileName += std::to_string(instance) ;
  return fileName ;
}

static int status()
{
  std::string NIFDFile = NIFDDevFile();

  int fd = open(NIFDFile.c_str(), O_RDWR);
  if (fd < 0) 
  {
    std::cout << "NIFD IP not available on selected device" << std::endl;
    return 0;
  }

  const int NIFD_CHECK_STATUS = 8;
  unsigned int status = 0;
  int result = ioctl(fd, NIFD_CHECK_STATUS, &status);
  if (result != 0)
  {
    std::cout << "ERROR: Could not read status register" << std::endl;
  }
  else
  {
    std::cout << "Current NIFD status: 0x" << std::hex << status << std::endl;
  }
  close(fd);
  return 0;
}

static int readback(const std::string& inputFile)
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

  std::string NIFDFile = NIFDDevFile();

  int fd = open(NIFDFile.c_str(), O_RDWR);
  if (fd < 0) 
  {
    std::cout << "NIFD IP not available on selected device" << std::endl;
    return 0;
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
  result = ioctl(fd, NIFD_SWITCH_ICAP_TO_NIFD);
  if (result != 0)
  {
    std::cout << "ERROR: Could not switch ICAP to NIFD control" << std::endl ;
    close(fd) ;
    return 0 ;
  }
  result = ioctl(fd, NIFD_READBACK_VARIABLE, packet) ;
  result |= ioctl(fd, NIFD_SWITCH_ICAP_TO_PR);
  if (result != 0)
  {
    std::cout << "ERROR: Could not readback variable!" << std::endl ;
    close(fd) ;
    return 0 ;
  }

  std::cout << "Value read: " ;
  for (unsigned int i = 0 ; i < resultWords ; ++i)
  {
    std::cout << std::hex << "0x" << packet[1 + numBits*2 + i] << " " ;
  }
  std::cout << std::endl ;

  close(fd) ;
  return 0 ;
}

#endif

int nifdHandler(int argc, char* argv[])
{
  sudoOrDie() ;
  
  if (argc < 2)
    return -EINVAL ;

  std::string op = argv[1] ;
  if (op.compare("--status") == 0)
    return status() ;
  if (op.compare("--readback") == 0)
  {
    if (argc < 3) 
      return -EINVAL ;
    return readback(argv[2]) ;
  }

  return -EINVAL ;
}
