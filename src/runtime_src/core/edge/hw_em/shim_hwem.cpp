/**
 * Copyright (C) 2016-2019 Xilinx, Inc
 * Author(s): Ch Vamshi Krishna
 *          : Hemant Kashyap
 * ZNYQ HAL sw_emu Driver layered on top of ZYNQ hardware driver
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

#include "pllauncher_defines.h"
#include <iostream>
#include <boost/property_tree/xml_parser.hpp>
#include "shim.h"
#include <cstring>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <thread>
#include "core/common/config_reader.h"
#include "core/common/xclbin_parser.h"

namespace ZYNQ {

namespace ZYNQ_HW_EM {
bool isRemotePortMapped = false;
void * remotePortMappedPointer = NULL;
namespace pt = boost::property_tree;

bool initRemotePortMap()
{
  int fd;
  unsigned addr;//, page_addr , page_offset;
  unsigned page_size = sysconf(_SC_PAGESIZE);

  fd = open("/dev/mem", O_RDWR);
  if (fd < 1) {
    std::cout << "Unable to open /dev/mem file" << std::endl;
    exit(-1);
  }

  #if defined(CONFIG_ARM64)
    addr = PL_RP_MP_ALLOCATED_ADD;
  #else
    addr = PL_RP_ALLOCATED_ADD;
  #endif

  //page_addr = (addr & ~(page_size - 1));
  //page_offset = addr - page_addr;

  remotePortMappedPointer = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
  MAP_SHARED, fd, (addr & ~(page_size - 1)));

  if (remotePortMappedPointer == MAP_FAILED) {
    std::cout << "Remote Port mapping to address " << addr << " Failed" << std::endl;
    exit(-1);
  }
  ZYNQ_HW_EM::isRemotePortMapped = true;
  return true;
}

bool validateXclBin(const xclBin *header , std::string &xclBinName)
{

  char *bitstreambin = reinterpret_cast<char*> (const_cast<xclBin*> (header));

    //int result = 0; Not used. Removed to get rid of compiler warning, and probably a Coverity CID.
    ssize_t zipFileSize = 0;
    ssize_t xmlFileSize = 0;
    ssize_t debugFileSize = 0;
    ssize_t memTopologySize = 0;

    char* xmlFile = nullptr;

    if ((!std::memcmp(bitstreambin, "xclbin0", 7)) || (!std::memcmp(bitstreambin, "xclbin1", 7)))
    {
      return false;
    }
    else if (!std::memcmp(bitstreambin,"xclbin2",7))
    {
      auto top = reinterpret_cast<const axlf*>(header);
      if (auto sec = xclbin::get_axlf_section(top,EMBEDDED_METADATA)) {
        xmlFileSize = sec->m_sectionSize;
        xmlFile = new char[xmlFileSize];
        memcpy(xmlFile, bitstreambin + sec->m_sectionOffset, xmlFileSize);
      }
    }
    else
    {
      return false;
    }

    if(!xmlFile)
    {
      return false;
    }

    pt::ptree xml_project;
    std::string sXmlFile;
    sXmlFile.assign(xmlFile,xmlFileSize);
    std::stringstream xml_stream;
    xml_stream<<sXmlFile;
    pt::read_xml(xml_stream,xml_project);

    // iterate platforms
    int count = 0;
    for (auto& xml_platform : xml_project.get_child("project"))
    {
      if (xml_platform.first != "platform")
        continue;
      if (++count>1)
      {
        //Give error and return from here
      }
    }

    // iterate devices
    count = 0;
    for (auto& xml_device : xml_project.get_child("project.platform"))
    {
      if (xml_device.first != "device")
        continue;
      if (++count>1)
      {
        //Give error and return from here
      }
    }

    // iterate cores
    count = 0;
    for (auto& xml_core : xml_project.get_child("project.platform.device"))
    {
      if (xml_core.first != "core")
        continue;
      if (++count>1)
      {
        //Give error and return from here
      }
    }
    xclBinName = xml_project.get<std::string>("project.<xmlattr>.name","");

    return true;

}
}

int shim::xclLoadXclBin(const xclBin *header)
{
  int ret = 0;
 /*if (mLogStream.is_open()) {
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

  if (!ZYNQ_HW_EM::validateXclBin(header, xclBinName)) {
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
  memcpy((char*) (ZYNQ_HW_EM::remotePortMappedPointer), &cPacketEndChar, 1);*/

  /* for emulation, we don't download */
  mKernelClockFreq = xrt_core::xclbin::get_kernel_freq(header);
  drm_zocl_axlf axlf_obj = {
    .za_xclbin_ptr = const_cast<axlf *>(header),
    .za_flags = 0,
  };

  ret = ioctl(mKernelFD, DRM_IOCTL_ZOCL_READ_AXLF, &axlf_obj);

  return ret;
}

shim::~shim() {
  //Tell the Pllauncher to close
  if (ZYNQ_HW_EM::isRemotePortMapped) {
    auto cmd = std::make_unique<PLLAUNCHER::OclCommand>();
    cmd->setCommand(PLLAUNCHER::PL_OCL_XRESET_ID);
    uint32_t iLength;
    memcpy((char*) (ZYNQ_HW_EM::remotePortMappedPointer), (char*) cmd->generateBuffer(&iLength),
        iLength);

    //Send the end of packet
    char cPacketEndChar = PL_OCL_PACKET_END_MARKER;
    memcpy((char*) (ZYNQ_HW_EM::remotePortMappedPointer), &cPacketEndChar, 1);
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


