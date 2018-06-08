/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 * Author: chvamshi
 * ZNYQ HAL Driver layered on top of ZYNQ kernel driver
 */

#include "pllauncher_defines.h"
#include <iostream>
#include "shim.cpp"
#include "HPIXclbinXmlReaderWriterLMX.h"
#include "xclbin.h"
#include "lmx6.0/lmxparse.h"
#include <cstring>
namespace ZYNQ {

namespace ZYNQ_HW_EM {
bool isRemotePortMapped = false;
void * remotePortMappedPointer = NULL;
bool initRemotePortMap() {
  int fd;
  unsigned addr, page_addr, page_offset;
  unsigned page_size = sysconf(_SC_PAGESIZE);

  fd = open("/dev/mem", O_RDWR);
  if (fd < 1) {
    std::cout << "Unable to open /dev/mem file" << std::endl;
    exit(-1);
  }

#ifdef RDIPF_aarch64
  addr = PL_RP_MP_ALLOCATED_ADD;
#endif
#ifdef RDIPF_arm64
  addr = PL_RP_ALLOCATED_ADD;
#endif

  page_addr = (addr & ~(page_size - 1));
  page_offset = addr - page_addr;

  remotePortMappedPointer = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
  MAP_SHARED, fd, (addr & ~(page_size - 1)));

  if (remotePortMappedPointer == MAP_FAILED) {
    std::cout << "Remote Port mapping to address " << addr << " Failed" << std::endl;
    exit(-1);
  }
  ZYNQ_HW_EM::isRemotePortMapped = true;
  return true;
}

bool validateXclBin(const char* xmlfile, uint32_t xmlFileSize, Xclbin::Platform& platform,
    Xclbin::Core& core, std::string &xclBinName) {
  //read the xclbin xml file and create compute units.
  //parse XML with LMX from temporary file
  bool errorStatus = true;
  LMX60_NS::elmx_error l_error;
  try {
    Xclbin::Project project(xmlfile, xmlFileSize, &l_error);
    std::stringstream xclName;
    xclName << project.GetName();
    xclBinName = xclName.str();

    //check single platform, single device XCLBIN files
    if (!(project.SizePlatform() == 1 && project.GetPlatform(0).SizeDevice() == 1)) {
      return errorStatus;
    }
    const Xclbin::Device & device = project.GetPlatform(0).GetDevice(0);

    //check single core
    if (!(device.SizeCore() == 1)) {
      return errorStatus;
    }

    platform = project.GetPlatform(0);
    core = device.GetCore(0);

    //check core type is clc_region
    {
      std::string coretype = core.GetType();
      if (coretype != "clc_region") {
        return errorStatus;
      }
    }
  } catch (const LMX60_NS::c_lmx_exception& err) {

	  return errorStatus;
  }
  return false;
}
}

int ZYNQShim::xclLoadXclBin(const xclBin *header) {
  int ret = 0;
  if (mLogStream.is_open()) {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    mLogStream.close();
  }
  char *bitstreambin = reinterpret_cast<char*>(const_cast<xclBin*>(header));
  std::string xclBinName = "";
  if (!ZYNQ_HW_EM::isRemotePortMapped) {
    ZYNQ_HW_EM::initRemotePortMap();
  }
  ssize_t xmlFileSize = 0;
  char* xmlFile = nullptr;

  if ((!std::memcmp(bitstreambin, "xclbin0", 7)) || (!std::memcmp(bitstreambin, "xclbin1", 7))) {

	printf("ERROR: Legacy xclbins are no longer supported. \n");
    return 1;

  } else if (!std::memcmp(bitstreambin, "xclbin2", 7)) {
    auto top = reinterpret_cast<const axlf*>(header);
    if (auto sec = xclbin::get_axlf_section(top, EMBEDDED_METADATA)) {
      xmlFileSize = sec->m_sectionSize;
      xmlFile = new char[xmlFileSize + 1];
      memcpy(xmlFile, bitstreambin + sec->m_sectionOffset, xmlFileSize);
      xmlFile[xmlFileSize] = '\0';
    }
  } else {
    return 1;
  }

  Xclbin::Core core;
  Xclbin::Platform platform;
  if (ZYNQ_HW_EM::validateXclBin(xmlFile, xmlFileSize, platform, core, xclBinName)) {
    printf("ERROR:Xclbin validation failed\n");
    return 1;
  }

  xclBinName = xclBinName + ".xclbin";

  //Send the LoadXclBin
  PLLAUNCHER::OclCommand *cmd = new PLLAUNCHER::OclCommand();
  cmd->setCommand(PLLAUNCHER::PL_OCL_LOADXCLBIN_ID);
  cmd->addArg(xclBinName.c_str());
  uint32_t length;
  uint8_t* buff = cmd->generateBuffer(&length);
  for (unsigned int i = 0; i < length; i += 4) {
    uint32_t copySize = (length - i) > 4 ? 4 : length - i;
    memcpy(((char*) (ZYNQ_HW_EM::remotePortMappedPointer)) + i, buff + i, copySize);
  }

  //Send the end of packet
  char cPacketEndChar = PL_OCL_PACKET_END_MARKER;
  memcpy((char*) (ZYNQ_HW_EM::remotePortMappedPointer), &cPacketEndChar, 1);
  return ret;
}

ZYNQShim::~ZYNQShim() {
  //Tell the Pllauncher to close
  if (ZYNQ_HW_EM::isRemotePortMapped) {
    PLLAUNCHER::OclCommand *cmd = new PLLAUNCHER::OclCommand();
    cmd->setCommand(PLLAUNCHER::PL_OCL_XRESET_ID);
    uint32_t iLength;
    memcpy((char*) (ZYNQ_HW_EM::remotePortMappedPointer), (char*) cmd->generateBuffer(&iLength),
        iLength);

    //Send the end of packet
    char cPacketEndChar = PL_OCL_PACKET_END_MARKER;
    memcpy((char*) (ZYNQ_HW_EM::remotePortMappedPointer), &cPacketEndChar, 1);
    delete cmd;
  }
  if (mKernelFD > 0) {
    close(mKernelFD);
  }
  if (mLogStream.is_open()) {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    mLogStream.close();
  }
}

}
unsigned int xclProbe() {
  return 1;
}
//end namespace ZYNQ


