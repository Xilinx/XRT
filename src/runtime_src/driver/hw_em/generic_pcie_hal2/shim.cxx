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

// Copyright 2014 Xilinx, Inc. All rights reserved.
//
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.

#include "shim.h"
#include <sys/wait.h>

namespace xclhwemhal2 {

  std::map<unsigned int, HwEmShim*> devices;
  std::map<std::string, std::string> HwEmShim::mEnvironmentNameValueMap(xclemulation::getEnvironmentByReadingIni());
  std::ofstream HwEmShim::mDebugLogStream;
  bool HwEmShim::mFirstBinary = true;
  unsigned int HwEmShim::mBufferCount = 0;
  const int xclhwemhal2::HwEmShim::SPIR_ADDRSPACE_PRIVATE   = 0;
  const int xclhwemhal2::HwEmShim::SPIR_ADDRSPACE_GLOBAL    = 1;
  const int xclhwemhal2::HwEmShim::SPIR_ADDRSPACE_CONSTANT  = 2;
  const int xclhwemhal2::HwEmShim::SPIR_ADDRSPACE_LOCAL     = 3;
  const int xclhwemhal2::HwEmShim::SPIR_ADDRSPACE_PIPES     = 4;
  const unsigned HwEmShim::CONTROL_AP_START = 1;
  const unsigned HwEmShim::CONTROL_AP_DONE  = 2;
  const unsigned HwEmShim::CONTROL_AP_IDLE  = 4;

  Event::Event()
  {
    awlen = 0;
    arlen = 0 ;
    eventflags = 0;
    timestamp = 0;
    host_timestamp =0;
    readBytes = 0;
    writeBytes = 0;
  }

  size_t HwEmShim::alloc_void(size_t new_size) 
  {
    if (buf_size == 0) 
    {
      buf = malloc(new_size);
      return new_size;
    }
    if (buf_size < new_size) 
    {
      buf = (void*) realloc(buf,new_size);
      return new_size;
    }
    return buf_size;
  }

  static void saveWaveDataBases()
  {
    std::map<unsigned int, HwEmShim*>::iterator start = devices.begin();
    std::map<unsigned int, HwEmShim*>::iterator end = devices.end();
    for(; start != end; start++)
    {
      HwEmShim* handle = (*start).second;
      if(!handle)
        continue;
      handle->saveWaveDataBase();
      systemUtil::makeSystemCall(handle->deviceDirectory, systemUtil::systemOperation::REMOVE);
    }

  }
  static void sigHandler(int sn, siginfo_t *si, void *sc)
  {
    switch(sn) {
      case SIGSEGV:
        {
          saveWaveDataBases();
          kill(0,SIGSEGV);
          exit(1);
          break;
        }
      case SIGFPE :
        {
          saveWaveDataBases();
          kill(0,SIGTERM);
          exit(1);
          break;
        }
      case SIGABRT:
        {
          saveWaveDataBases();
          kill(0,SIGABRT);
          exit(1);
          break;
        }
      default:
        {
          break;
        }
    }
  }

  static void printMem(std::ofstream &os, int base, uint64_t offset, void* buf, unsigned int size )
  {
    if(os.is_open())
    {
      for(uint64_t i = 0; i < size ; i = i + base)
      {
        os << "@" << std::hex << offset + i <<std::endl;
        for(int j = base-1 ;j >=0 ; j--)
          os << std::hex << std::setfill('0') << std::setw(2) << (unsigned int)(((unsigned char*)buf)[i+j]);
        os << std::endl;
      }
    }
  }

  bool HwEmShim::isUltraScale() const
  {
    return false;
  }

 void HwEmShim::populateKernelArgInfo(const Xclbin::Kernel& kernel, std::map<uint64_t,KernelArg>& kernelArgInfo)
  {
    std::string kernelName = kernel.GetName();
    for (unsigned int l = 0; l < kernel.SizeArg(); ++l ) 
    {
      const Xclbin::Arg & arg = kernel.GetArg(l);
      std::string name =  arg.GetName() ;
      unsigned int qual = arg.GetAddressQualifier();
      std::string port = arg.GetPort();
      std::string argoffset = arg.GetOffset();
      unsigned int offset = std::strtoul(argoffset.c_str(), 0, 0);
      std::string argsize =  arg.GetSize();
      unsigned int size = std::strtoul(argsize.c_str(), 0, 0);
      std::string arghostoffset = arg.GetHostOffset() ;
      std::string arghostsize = arg.GetHostSize();
      std::string origuse =  arg.GetOrigUse() ;
      KernelArg kArg;
      kArg.name = kernelName + ":" + name;
      kArg.size = size;
      //workaround : set global, local, pipe args to width 8
      if((origuse!="function") &&
          ((qual==xclhwemhal2::HwEmShim::SPIR_ADDRSPACE_GLOBAL) ||
           (qual==xclhwemhal2::HwEmShim::SPIR_ADDRSPACE_LOCAL) ||
           (qual==xclhwemhal2::HwEmShim::SPIR_ADDRSPACE_CONSTANT) ||
           (qual==xclhwemhal2::HwEmShim::SPIR_ADDRSPACE_PIPES)))
      {
        if( mLogStream.is_open() )
        {
          mLogStream << __func__ << " TEMPORARY WORKAROUND : csim global size_t extending user arg " << l << " to 8" << std::endl;
        }
        size=8;
      }
      if((origuse=="function") && (name=="printf_buffer"))
      {
        if( mLogStream.is_open() )
        {
          mLogStream << __func__ << " TEMPORARY WORKAROUND : csim global size_t extending printf_buffer " << l << " to 8" << std::endl;
        }
        size=8;
      }
      //end workaround
      kernelArgInfo[offset] = kArg;
    }
  }
bool HwEmShim::getSaxiControlRemap(const Xclbin::Instance &instance, size_t &saxiControlMap)
  {
    bool saxiControlMapFound=false;
    for (unsigned int m = 0; m < instance.SizeAddrRemap(); ++m ) 
    {
      const Xclbin::AddrRemap & addrRemap = instance.GetAddrRemap(m);
      std::string port = addrRemap.GetPort() ;
      if(port=="S_AXI_CONTROL")
      {
        std::string sBase =  addrRemap.GetBase() ;
        saxiControlMap = std::strtoul(sBase.c_str(), 0, 0);
        saxiControlMapFound=true;
        break;
      }
    }
    return saxiControlMapFound;

  }

  size_t HwEmShim::getMinSaxiControlReMap(Xclbin::Core& core)
  {
    size_t minSaxiControlMap = SIZE_MAX;
    bool min_s_axi_control_remap_set = false;
    for ( unsigned k = 0; k < core.SizeKernel(); ++k ) 
    {
      const Xclbin::Kernel &kernel = core.GetKernel(k);
      for ( unsigned l = 0; l < kernel.SizeInstance(); ++l ) 
      {
        const Xclbin::Instance & instance = kernel.GetInstance(l);
        std::string instancename = instance.GetName() ;
        for ( unsigned m = 0; m < instance.SizeAddrRemap(); ++m ) 
        {
          const Xclbin::AddrRemap & addrRemap = instance.GetAddrRemap(m);
          std::string port = addrRemap.GetPort();
          std::string sBase =  addrRemap.GetBase();
          if(port=="S_AXI_CONTROL")
          {
            size_t base = std::strtoul(sBase.c_str(), 0, 0);
            if(base<minSaxiControlMap)
            {
              minSaxiControlMap=base;
              min_s_axi_control_remap_set=true;
            }
          }
        }
      }
    }

    if(!min_s_axi_control_remap_set) 
      minSaxiControlMap=0;
    return minSaxiControlMap;
  }

  bool HwEmShim::validateXclBin(const std::string& xmlfileName, Xclbin::Platform& platform,
      Xclbin::Core& core, std::string &xclBinName)
  {
    //read the xclbin xml file and create compute units.
    //parse XML with LMX from temporary file
    bool errorStatus = true;
    if( mLogStream.is_open() )
    {
      mLogStream << __func__ << " begin parsing XML " << std::endl;
    }
    LMX60_NS::elmx_error l_error;
    Xclbin::Project project;
    try {
      Xclbin::Project project_( xmlfileName.c_str(), &l_error );
      project = project_;
    }
    catch ( const LMX60_NS::c_lmx_exception& err ) {
      if( mLogStream.is_open() )
      {
        mLogStream << __func__ << " Failed to parse " << xmlfileName << std::endl;
      }
      return errorStatus;
    }
    xclBinName = project.GetName();

    //check single platform, single device XCLBIN files
    if(!(project.SizePlatform()==1 && project.GetPlatform(0).SizeDevice()==1))
    {
      if( mLogStream.is_open() )
      {
        mLogStream << __func__ << " Cannot handle multi platform or device XCLBIN file " << std::endl;
      }
      return errorStatus;
    }
    const Xclbin::Device & device = project.GetPlatform(0).GetDevice(0); 

    //check single core
    if(!(device.SizeCore() == 1))
    {
      if( mLogStream.is_open() )
      {
        mLogStream << __func__ << " Cannot handle multiple core XCLBIN file " << std::endl;
      }
      return errorStatus;
    }
    
    platform = project.GetPlatform(0);
    core = device.GetCore(0);

    //check core type is clc_region
    {
      std::string coretype =  core.GetType();
      if(coretype!="clc_region")
      {
        if( mLogStream.is_open() )
        {
          mLogStream << __func__ << " Cannot handle coretype= " << coretype  << std::endl;
        }
        return errorStatus;
      }
    }
    return false;
  }

  int HwEmShim::xclLoadXclBin(const xclBin *header)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }
    char *bitstreambin = reinterpret_cast<char*> (const_cast<xclBin*> (header));

    //int result = 0; Not used. Removed to get rid of compiler warning, and probably a Coverity CID.
    ssize_t zipFileSize = 0;
    ssize_t xmlFileSize = 0;
    ssize_t debugFileSize = 0;
    ssize_t memTopologySize = 0;

    char* zipFile = nullptr;
    char* xmlFile = nullptr;
    char* debugFile = nullptr;
    char* memTopology = nullptr;

    if ((!std::memcmp(bitstreambin, "xclbin0", 7)) || (!std::memcmp(bitstreambin, "xclbin1", 7)))
    {
      PRINTENDFUNC;
      return -1;
//      if (header->m_primaryFirmwareLength == 0) {
//        PRINTENDFUNC;
//        return -1;
//      }
//      if (isUltraScale() && (header->m_secondaryFirmwareLength == 0)) {
//        PRINTENDFUNC;
//        return -1;
//      }
//
//      zipFileSize = header->m_primaryFirmwareLength;
//      zipFile = new char[zipFileSize+1];
//      memcpy(zipFile, bitstreambin + header->m_primaryFirmwareOffset, zipFileSize);
//      zipFile[zipFileSize] = 0;      
//
//      xmlFileSize = header->m_metadataLength;
//      xmlFile = new char[xmlFileSize+1];
//      memcpy(xmlFile , bitstreambin + header->m_metadataOffset, xmlFileSize);
//      xmlFile[xmlFileSize] = '\0';

    }
    else if (!std::memcmp(bitstreambin,"xclbin2",7)) 
    {
      auto top = reinterpret_cast<const axlf*>(header);
      if (auto sec = xclbin::get_axlf_section(top,EMBEDDED_METADATA)) {
        xmlFileSize = sec->m_sectionSize;
        xmlFile = new char[xmlFileSize+1];
        memcpy(xmlFile, bitstreambin + sec->m_sectionOffset, xmlFileSize);
        xmlFile[xmlFileSize] = 0;      
      }
      if (auto sec = xclbin::get_axlf_section(top,BITSTREAM)) {
        zipFileSize = sec->m_sectionSize;
        zipFile = new char[zipFileSize+1];
        memcpy(zipFile, bitstreambin + sec->m_sectionOffset, zipFileSize);
        zipFile[zipFileSize] = 0;      
      }
      if (auto sec = xclbin::get_axlf_section(top,DEBUG_IP_LAYOUT)) {
        debugFileSize = sec->m_sectionSize;
        debugFile = new char[debugFileSize+1];
        memcpy(debugFile, bitstreambin + sec->m_sectionOffset, debugFileSize);
        debugFile[debugFileSize] = 0;
      }
      if (auto sec = xclbin::get_axlf_section(top,MEM_TOPOLOGY)) {
        memTopologySize = sec->m_sectionSize;
        memTopology = new char[memTopologySize+1];
        memcpy(memTopology, bitstreambin + sec->m_sectionOffset, memTopologySize);
        memTopology[memTopologySize] = 0;
      }
    //      goto done;
    //    xdev->topology.size = memHeader->m_sectionSize;

    //    get_user(bank_count, buffer);
    //    xdev->topology.bank_count = bank_count;
    //    buffer += offsetof(struct mem_topology, m_mem_data);
    //    xdev->topology.m_data_length = bank_count*sizeof(struct mem_data);
    //    xdev->topology.m_data = vmalloc(xdev->topology.m_data_length);
    //    err = copy_from_user(xdev->topology.m_data, buffer, bank_count*sizeof(struct mem_data));

    //    memTopology = new char[memTopologySize+1];
    //    memcpy(memTopology, bitstreambin + sec->m_sectionOffset, memTopologySize);
    //    memTopology[memTopologySize] = 0;      
    //  }
    }
    else
    {
      PRINTENDFUNC;
      return -1;
    }

    int returnValue = xclLoadBitstreamWorker(zipFile,zipFileSize+1,xmlFile,xmlFileSize+1,debugFile,debugFileSize+1, memTopology, memTopologySize+1);

    //mFirstBinary is a static member variable which becomes false once first binary gets loaded
    if(returnValue >=0 && mFirstBinary )
    {
      HwEmShim::mDebugLogStream.open(xclemulation::getEmDebugLogFile(),std::ofstream::out);
      if(xclemulation::config::getInstance()->isInfoSuppressed() == false)
      {
        std::string initMsg ="INFO: [SDx-EM 01] Hardware emulation runs simulation underneath. Using a large data set will result in long simulation times. It is recommended that a small dataset is used for faster execution. This flow does not use cycle accurate models and hence the performance data generated is approximate.";
        logMessage(initMsg);
        
        //following function has to be called only once... as it has a startThread guard, we are calling all the times
      }
      mFirstBinary = false;
    }
    mCore = new exec_core;
    mMBSch = new MBScheduler(this);
    mMBSch->init_scheduler_thread();
    
    delete[] zipFile;
    delete[] debugFile;
    delete[] xmlFile;
    delete[] memTopology;
    PRINTENDFUNC;
    return returnValue;
  }

//  int HwEmShim::xclLoadBitstream(const char *fileName)
//  {
//    if (mLogStream.is_open()) {
//      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << fileName << std::endl;
//    }
//
//    std::FILE *bit_file = std::fopen(fileName, "r");
//
//    if (!bit_file)
//    {
//      std::fclose(bit_file);
//      PRINTENDFUNC;
//      return -1;
//    }
//
//    xclBin header;
//    if (fread((void *)&header, sizeof(header), 1, bit_file) != 1)
//    {
//      std::fclose(bit_file);
//      PRINTENDFUNC;
//      return -1;
//    }
//
//    int result = 0;
//    ssize_t zipFileSize = 0;
//    ssize_t xmlFileSize = 0;
//
//    if ((!std::memcmp(header.m_magic, "xclbin0", 8)) || (!std::memcmp(header.m_magic, "xclbin1", 8)))
//    {
//      if (header.m_primaryFirmwareLength == 0) {
//        // PR bit stream is missing
//        std::fclose(bit_file);
//        PRINTENDFUNC;
//        return -1;
//      }
//      if (isUltraScale() && (header.m_secondaryFirmwareLength == 0)) {
//        // Clear bit stream is missing
//        std::fclose(bit_file);
//        PRINTENDFUNC;
//        return -1;
//      }
//
//      zipFileSize = header.m_primaryFirmwareLength;
//      result = fseek(bit_file, header.m_primaryFirmwareOffset, SEEK_SET);
//      xmlFileSize = header.m_metadataLength;
//    }
//    else
//    {
//      fseek(bit_file, 0, SEEK_END);
//      zipFileSize = ftell(bit_file);
//      fseek(bit_file, 0, SEEK_SET);
//      result = fseek(bit_file, 0, SEEK_SET);
//    }
//
//    if (result)
//    {
//      std::fclose(bit_file);
//      PRINTENDFUNC;
//      return -1;
//    }
//
//    if(zipFileSize <= 0)
//    {
//      std::fclose(bit_file);
//      PRINTENDFUNC;
//      return -1;
//    }
//
//    char *zipFile = new char[zipFileSize+1];
//    if(!zipFile)
//    {
//      std::fclose(bit_file);
//      PRINTENDFUNC;
//      return -1;
//    }
//
//    size_t zipFileReadBytes = fread(zipFile,1,zipFileSize,bit_file);
//
//    if(zipFileReadBytes == 0)
//    {
//      std::fclose(bit_file);
//      delete[] zipFile;
//      PRINTENDFUNC;
//      return -1;
//    }
//    zipFile[zipFileReadBytes] = '\0';
//
//    result = fseek(bit_file, header.m_metadataOffset, SEEK_SET);
//    char *xmlFile = new char[xmlFileSize+1];
//    if(!xmlFile)
//    {
//      std::fclose(bit_file);
//      delete[] zipFile;
//      PRINTENDFUNC;
//      return -1;
//    }
//    size_t xmlFileReadBytes = fread(xmlFile,1,xmlFileSize,bit_file);
//    if(xmlFileReadBytes == 0)
//    {
//      std::fclose(bit_file);
//      delete[] xmlFile;
//      delete[] zipFile;
//      PRINTENDFUNC;
//      return -1;
//    }
//    std::fclose(bit_file);
//
//    int returnValue = xclLoadBitstreamWorker(zipFile,zipFileReadBytes+1,xmlFile,xmlFileReadBytes+1);
//    //mFirstBinary is a static member variable which becomes false once first binary gets loaded
//    if(returnValue >=0 && mFirstBinary )
//    {
//      HwEmShim::mDebugLogStream.open(xclemulation::getEmDebugLogFile(),std::ofstream::out);
//      if(xclemulation::config::getInstance()->isInfoSuppressed() == false)
//      {
//        std::string initMsg ="INFO: [SDx-EM 01] Hardware emulation runs detailed simulation underneath. It may take long time for large data set. Please use a small dataset for faster execution. You can still get performance trend for your kernel with smaller dataset." ;
//      logMessage(initMsg);
//      }
//      mFirstBinary = false;
//    }
//    delete[] zipFile;
//    delete[] xmlFile;
//    PRINTENDFUNC;
//    return returnValue;
//  }
 
   int HwEmShim::xclLoadBitstreamWorker(char* zipFile, size_t zipFileSize, char* xmlfile, size_t xmlFileSize,
                                        char* debugFile, size_t debugFileSize, char* memTopology, size_t memTopologySize)
  {
    if (mLogStream.is_open()) {
      //    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << zipFile<< std::endl;
    }

    //TBD the file read may slowdown things...whenever xclLoadBitStream hal API implementation changes, we also need to make changes.
    char fileName[1024];
#ifndef _WINDOWS
    // TODO: Windows build support
    //    getpid is defined in unistd.h
    std::sprintf(fileName, "%s/tempFile_%d", deviceDirectory.c_str(),binaryCounter);
#endif
    if(mMemModel)
    {
      delete mMemModel;
      mMemModel = NULL;
    }
    if(sock)
    {
      resetProgram();
    }
    std::stringstream ss;
    ss<<deviceDirectory<<"/binary_"<<binaryCounter;
    std::string binaryDirectory = ss.str();

    systemUtil::makeSystemCall(binaryDirectory, systemUtil::systemOperation::CREATE);

    std::ofstream os(fileName);

    os.write(zipFile, zipFileSize);
    os.close();

    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_flags = SA_SIGINFO;
    s.sa_sigaction = sigHandler;
    if (sigaction(SIGSEGV, &s, (struct sigaction *)0) ||
        sigaction(SIGFPE , &s, (struct sigaction *)0) ||
        sigaction(SIGABRT, &s, (struct sigaction *)0))
    {
      //debug_print("unable to support all signals");
    }

    std::string sim_path("");
    std::string sim_file("simulate.sh");

    // Write and read debug IP layout (for debug & profiling)
    // NOTE: for now, let's do this file based so we can debug
    std::string debugFileName = binaryDirectory + "/debug_ip_layout";
    FILE *fp2 = fopen(debugFileName.c_str(), "wb");
    if (fp2 == NULL) {
      if (mLogStream.is_open())
        mLogStream << __func__ << " failed to create temporary debug_ip_layout file " << std::endl;
      return -1;
    }

    if ((debugFile != nullptr) && (debugFileSize > 1))
      fwrite(debugFile, debugFileSize, 1, fp2);
    fflush(fp2);
    fclose(fp2);

    readDebugIpLayout(debugFileName);

    const mem_topology* m_mem = (reinterpret_cast<const ::mem_topology*>(memTopology));
    if(m_mem)
    {
      for (int32_t i=0; i<m_mem->m_count; ++i) 
      {
        std::string tag = reinterpret_cast<const char*>(m_mem->m_mem_data[i].m_tag);
        mMembanks.emplace_back (membank{m_mem->m_mem_data[i].m_base_address,tag,m_mem->m_mem_data[i].m_size*1024,i});
      }
      if(m_mem->m_count > 0)
      {
        mDDRMemoryManager.clear();
      }

      for(auto it:mMembanks )
      {
        //CR 966701: alignment to 4k (instead of mDeviceInfo.mDataAlignment)
        mDDRMemoryManager.push_back(new xclemulation::MemoryManager(it.size, it.base_addr, 4096));
        //std::cout<<"BASE "<<std::hex<< it.base_addr<<" TAG "<<it.tag<<" SIZE "<<it.size<<" INDEX "<<it.index<<std::endl;
      }
    }
    // Write XML metadata from xclbin
    std::string xmlFileName("");
    xmlFileName = binaryDirectory + "/xmltmp";
    bool xmlFileCreated=false;

    while(!xmlFileCreated)
    {
      FILE *fp=fopen(xmlFileName.c_str(),"rb");
      if(fp==NULL) xmlFileCreated=true;
      else 
      {
        fclose(fp);
        xmlFileName += std::string("_");
      }
    }
    FILE *fp=fopen(xmlFileName.c_str(),"wb");
    if(fp==NULL)
    {
      if (mLogStream.is_open()) 
      {
        mLogStream << __func__ << " failed to create temporary xml file " << std::endl;
      }
      return -1;
    }
    fwrite(xmlfile,xmlFileSize,1,fp);
    fflush(fp);
    fclose(fp);

    // Validate xclbin file and extract platform & core
    Xclbin::Platform platform;
    Xclbin::Core core;
    std::string xclBinName("");
    bool errorStatus = validateXclBin(xmlFileName, platform, core,xclBinName);
    if (errorStatus)
      return -1;
    
    set_simulator_started(true);
    bool simDontRun = xclemulation::config::getInstance()->isDontRun();
    char* simMode = NULL;
    std::string wdbFileName("");
    // The following is evil--hardcoding. This name may change.
    // Is there a way we can determine the name from the directories or otherwise?
    std::string bdName("dr"); // Used to be opencldesign. This is new default.
    if( !simDontRun )
    {
      wdbFileName = std::string(mDeviceInfo.mName) + "-" + std::to_string(mDeviceIndex) + "-" + xclBinName ;
      xclemulation::LAUNCHWAVEFORM lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();
      std::string userSpecifiedSimPath = xclemulation::config::getInstance()->getSimDir();
      if(userSpecifiedSimPath.empty())
      {
        std::string _sFilePath(fileName);
        systemUtil::makeSystemCall (_sFilePath, systemUtil::systemOperation::UNZIP, binaryDirectory);
        systemUtil::makeSystemCall (binaryDirectory, systemUtil::systemOperation::PERMISSIONS, "777");
      }
      
      if( lWaveform == xclemulation::LAUNCHWAVEFORM::GUI )
      {
        // NOTE: proto inst filename must match name in HPIKernelCompilerHwEmu.cpp
        std::string protoFileName = "./" + bdName + "_behav.protoinst";
        std::stringstream cmdLineOption;
        cmdLineOption << " --gui --wdb " << wdbFileName << ".wdb"
                      << " --protoinst " << protoFileName;

        simMode = strdup(cmdLineOption.str().c_str());
        sim_path = binaryDirectory+ "/behav_waveform/xsim";
        struct stat statBuf;
        if ( stat(sim_path.c_str(), &statBuf) != 0 )
        {
          sim_path = binaryDirectory+ "/behav_waveform/questa";
        }
        std::string generatedWcfgFileName = sim_path + "/" + bdName + "_behav.wcfg";
        unsetenv("SDX_LAUNCH_WAVEFORM_BATCH");
        setenv("SDX_WAVEFORM",generatedWcfgFileName.c_str(),true);
      }

      if(lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH )
      {
        // NOTE: proto inst filename must match name in HPIKernelCompilerHwEmu.cpp
        std::string protoFileName = "./" + bdName + "_behav.protoinst";
        std::stringstream cmdLineOption;
        cmdLineOption << " --wdb " << wdbFileName << ".wdb"
                      << " --protoinst " << protoFileName;

        simMode = strdup(cmdLineOption.str().c_str());
        sim_path = binaryDirectory+ "/behav_waveform/xsim";
        struct stat statBuf;
        if ( stat(sim_path.c_str(), &statBuf) != 0 )
        {
          sim_path = binaryDirectory+ "/behav_waveform/questa";
        }
        std::string generatedWcfgFileName = sim_path + "/" + bdName + "_behav.wcfg";
        setenv("SDX_LAUNCH_WAVEFORM_BATCH","1",true);
        setenv("SDX_WAVEFORM",generatedWcfgFileName.c_str(),true);
      }

      if(userSpecifiedSimPath.empty() == false)
      {
        sim_path = userSpecifiedSimPath;
      }
      else
      {
        if(sim_path.empty())
        {
          sim_path = binaryDirectory+ "/behav_gdb/xsim";
          struct stat statBuf1;
          if ( stat(sim_path.c_str(), &statBuf1) != 0 )
          {
            sim_path = binaryDirectory+ "/behav_gdb/questa";
          }
        }
        struct stat statBuf;
        if ( stat(sim_path.c_str(), &statBuf) != 0 )
        {
          std::string dMsg = "WARNING: [SDx-EM 07] None of the kernels is compiled in debug mode. Compile kernels in debug mode to launch waveform";
          logMessage(dMsg,0);
          sim_path = binaryDirectory+ "/behav_gdb/xsim";
          struct stat statBuf2;
          if ( stat(sim_path.c_str(), &statBuf2) != 0 )
          {
            sim_path = binaryDirectory+ "/behav_gdb/questa";
          }
        }
      }
      std::stringstream socket_id;
      socket_id << deviceName<<"_"<<binaryCounter<<"_";
#ifndef _WINDOWS
      // TODO: Windows build support
      //   getpid is defined in unistd.h
      //   setenv is defined in stdlib.h
      socket_id << getpid();
      setenv("EMULATION_SOCKETID",socket_id.str().c_str(),true);
#endif
      binaryCounter++;
    }
    if(deviceDirectory.empty() == false)
      setenv ("EMULATION_RUN_DIR", deviceDirectory.c_str(),true);


    // Create waveform config file
    // NOTE: see corresponding wdb file in saveWaveDataBase
    if(wdbFileName.empty() == false)
    {
      setenv("SDX_QUESTA_WLF_FILENAME",std::string(wdbFileName+".wlf").c_str(),true);
      mBinaryDirectories[sim_path] = wdbFileName;
    }

    //launch simulation
    if (!sim_path.empty()) {
#ifndef _WINDOWS
      // TODO: Windows build support
      //   pid_t, fork, chdir, execl is defined in unistd.h
      //   this environment variable is added to disable the systemc copyright message
      setenv("SYSTEMC_DISABLE_COPYRIGHT_MESSAGE","1",true);
      pid_t pid = fork();
      assert(pid >= 0);
      if (pid == 0){ //I am child
        //Redirecting the XSIM log to a file
        freopen("/dev/null","w",stdout);
        chdir(sim_path.c_str());

        // If the sdx server port was specified in the .ini file,
        //  we need to pass this information to the spawned xsim process.
        if (xclemulation::config::getInstance()->getServerPort() != 0)
        {
          std::stringstream convert ;
          convert << xclemulation::config::getInstance()->getServerPort() ;
          setenv("XILINX_SDX_SERVER_PORT", convert.str().c_str(), 1) ;
        }

        if (mLogStream.is_open())
          mLogStream << __func__ << " xocc command line: " << simMode << std::endl;

        int r = execl(sim_file.c_str(),sim_file.c_str(),simMode,NULL);
        fclose (stdout);
        if(r == -1){std::cerr << "FATAL ERROR : Simulation process did not launch" << std::endl; exit(1);}
        exit(0);
      }
#endif
    }
    //if platform is a XPR platform, dont serilize ddr memory
    if(isXPR())
    {
      mEnvironmentNameValueMap["enable_pr"] = "false";
    }
    sock = new unix_socket;
    if(sock && mEnvironmentNameValueMap.empty() == false)
    {
      //send environment information to device
      bool ack = true;
      xclSetEnvironment_RPC_CALL(xclSetEnvironment);
    }
    
    size_t minSaxiControlMap = getMinSaxiControlReMap(core);

    for ( unsigned k = 0; k < core.SizeKernel(); ++k ) 
    {
      const Xclbin::Kernel &kernel = core.GetKernel(k);
      std::string name = kernel.GetName();
      std::map<uint64_t, KernelArg> kernelArgInfo;
      populateKernelArgInfo(kernel,kernelArgInfo);
      
      //kernel instances
      for ( unsigned int l = 0; l < kernel.SizeInstance(); ++l ) 
      {
      //  //create cu object
        const Xclbin::Instance & instance = kernel.GetInstance(l);
        std::string instanceName=instance.GetName();

        uint64_t saxiControlMap = 0;
        // Return variable in the following was not being used. It has been
        // removed to get rid of a compiler warning, and probably a Coverity CID.
        //bool saxiControlMapFound = getSaxiControlRemap(instance,saxiControlMap);
        getSaxiControlRemap(instance, saxiControlMap);
        
        if (xclemulation::config::getInstance()->isMemLogsEnabled())
        {
          std::ofstream* controlStream = new std::ofstream;
          controlStream->open( instanceName + "_control.mem" );
          mOffsetInstanceStreamMap[saxiControlMap] = controlStream;
        }

        mKernelOffsetArgsInfoMap[saxiControlMap] = kernelArgInfo;
      }
    }

    if(simMode)
    {
      free(simMode);
    }
    return 0;
  }

   size_t HwEmShim::xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size) {
     
     if (!simulator_started)
       return 0;
     
     if (mLogStream.is_open()) {
       mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << space << ", "
         << offset << ", " << hostBuf << ", " << size << std::endl;
     }
     switch (space) {
       case XCL_ADDR_SPACE_DEVICE_RAM:
         {
           const size_t totalSize = size;
           const size_t mod_size1 = offset % DDR_BUFFER_ALIGNMENT;
           const size_t mod_size2 = size % DDR_BUFFER_ALIGNMENT;
           if (mod_size1) {
             // Buffer not aligned at DDR_BUFFER_ALIGNMENT boundary, need to do Read-Modify-Write
             size_t returnVal = xclReadModifyWrite(offset, hostBuf, size);
             PRINTENDFUNC;
             return returnVal;
           }
           else if (mod_size2) {
             // Buffer not a multiple of DDR_BUFFER_ALIGNMENT, write out the initial block and
             // then perform a Read-Modify-Write for the remainder buffer
             const size_t blockSize = size - mod_size2;
             if (xclWrite(space, offset, hostBuf, blockSize) != blockSize)
             {
               PRINTENDFUNC;
               return -1;
             }
             offset += blockSize;
             hostBuf = static_cast<const char*>(hostBuf) + blockSize;
             if (xclReadModifyWrite(offset, hostBuf, mod_size2) != mod_size2)
             {
               PRINTENDFUNC;
               return -1;
             }
             PRINTENDFUNC;
             return totalSize;
           }

           const char *curr = (const char *)hostBuf;
           xclWriteAddrSpaceDeviceRam_RPC_CALL(xclWriteAddrSpaceDeviceRam ,space,offset,curr,size);
           PRINTENDFUNC;
           return totalSize;
         }
       case XCL_ADDR_SPACE_DEVICE_PERFMON:
         {
           PRINTENDFUNC;
           return -1;
         }
       case XCL_ADDR_SPACE_DEVICE_CHECKER:
         {
           PRINTENDFUNC;
           return -1;
         }
       case XCL_ADDR_KERNEL_CTRL:
         {
           std::map<uint64_t,std::pair<std::string,unsigned int>> offsetArgInfo;
           unsigned int paddingFactor = xclemulation::config::getInstance()->getPaddingFactor();

           std::string kernelName("");
           uint32_t *hostBuf32 = ((uint32_t*)hostBuf);
          // if(hostBuf32[0] & CONTROL_AP_START)
           {
             auto offsetKernelArgInfoItr = mKernelOffsetArgsInfoMap.find(offset);
             if(offsetKernelArgInfoItr != mKernelOffsetArgsInfoMap.end())
             {
               unsigned char* axibuf=((unsigned char*) hostBuf);
               std::map<uint64_t, KernelArg> kernelArgInfo = (*offsetKernelArgInfoItr).second;
               for (auto i : kernelArgInfo) 
               {
                 uint64_t argOffset = i.first;
                 KernelArg kArg = i.second;
                 uint64_t argPointer = 0;
                 std::memcpy(&argPointer,axibuf+ argOffset,kArg.size);
                 std::map<uint64_t,uint64_t>::iterator it = mAddrMap.find(argPointer);
                 if(it != mAddrMap.end())
                 {
                   uint64_t offsetSize =  (*it).second; 
                   uint64_t padding = (paddingFactor == 0) ? 0 : offsetSize/(1+(paddingFactor*2));
                   std::pair<std::string,unsigned int> sizeNamePair(kArg.name,offsetSize); 
                   if(hostBuf32[0] & CONTROL_AP_START)
                     offsetArgInfo[argPointer-padding] = sizeNamePair;
                   size_t pos = kArg.name.find(":");
                   if(pos != std::string::npos)
                   {
                     kernelName = kArg.name.substr(0,pos);
                   }
                 }
               }
             }
           }
           
           auto controlStreamItr = mOffsetInstanceStreamMap.find(offset);
           if(controlStreamItr != mOffsetInstanceStreamMap.end())
           {
             std::ofstream* controlStream = (*controlStreamItr).second;
             if(hostBuf32[0] & CONTROL_AP_START)
               printMem(*controlStream,4, offset, (void*)hostBuf, 4 );
             else
               printMem(*controlStream,4, offset, (void*)hostBuf, size );
           }

           if(hostBuf32[0] & CONTROL_AP_START)
           {
             std::string dMsg ="INFO: [SDx-EM 04-0] Sending start signal to the kernel " + kernelName;
             logMessage(dMsg,1);
           }
           else
           {
             std::string dMsg ="INFO: [SDx-EM 03-0] Configuring registers for the kernel " + kernelName +" Started";
             logMessage(dMsg,1);
           }
           // print populate info
           /*    for(auto itr:offsetArgInfo)
                 {
                 std::cout<<"offset "<<itr.first<<" size "<<itr.second.second<<" name "<<itr.second.first<<std::endl;
                 }*/
           xclWriteAddrKernelCtrl_RPC_CALL(xclWriteAddrKernelCtrl,space,offset,hostBuf,size,offsetArgInfo);
           if(hostBuf32[0] & CONTROL_AP_START)
           {
             std::string dMsg ="INFO: [SDx-EM 04-1] Kernel " + kernelName +" is Started";
             logMessage(dMsg,1);
           }
           else
           {
             std::string dMsg ="INFO: [SDx-EM 03-1] Configuring registers for the kernel " + kernelName +" Ended";
             logMessage(dMsg,1);
           }
           PRINTENDFUNC;
           return size;
         }
       default:
         {
           PRINTENDFUNC;
           return -1;
         }
     }

   }

  size_t HwEmShim::xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size) {
    
    uint32_t no_of_final_samples = 0;

    if(tracecount_calls < xclemulation::config::getInstance()->getMaxTraceCount())
    {
      tracecount_calls = tracecount_calls + 1;
      return 0;
    }
    tracecount_calls = 0;

    if (!simulator_started)
      return 0;
    
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << space << ", "
        << offset << ", " << hostBuf << ", " << size << std::endl;
    }
    switch (space) {
      case XCL_ADDR_SPACE_DEVICE_RAM:
        {
          const size_t mod_size1 = offset % DDR_BUFFER_ALIGNMENT;
          const size_t mod_size2 = size % DDR_BUFFER_ALIGNMENT;
          const size_t totalSize = size;

          if (mod_size1) {
            // Buffer not aligned at DDR_BUFFER_ALIGNMENT boundary, need to do Read-Skip-Copy
            size_t returnVal = xclReadSkipCopy(offset, hostBuf, size);
            PRINTENDFUNC;
            return returnVal;
          }
          else if (mod_size2) {
            // Buffer not a multiple of DDR_BUFFER_ALIGNMENT, read the initial block and
            // then perform a Read-Skip-Copy for the remainder buffer
            const size_t blockSize = size - mod_size2;
            if (xclRead(space, offset, hostBuf, blockSize) != blockSize)
            {
              PRINTENDFUNC;
              return -1;
            }
            offset += blockSize;
            hostBuf = static_cast<char*>(hostBuf) + blockSize;
            if (xclReadSkipCopy(offset, hostBuf, mod_size2) != mod_size2)
            {
              PRINTENDFUNC;
              return -1;
            }
            PRINTENDFUNC;
            return totalSize;
          }

          //const char *curr = (const char *)hostBuf;
          xclReadAddrSpaceDeviceRam_RPC_CALL(xclReadAddrSpaceDeviceRam,space,offset,hostBuf,size);
          PRINTENDFUNC;
          return totalSize;
        }
      case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
          PRINTENDFUNC;
          return -1;
        }
       case XCL_ADDR_SPACE_DEVICE_CHECKER:
         {
           PRINTENDFUNC;
           return -1;
         }
      case XCL_ADDR_KERNEL_CTRL:
        {
          xclGetDebugMessages();
          xclReadAddrKernelCtrl_RPC_CALL(xclReadAddrKernelCtrl,space,offset,hostBuf,size);
          PRINTENDFUNC;
          return size;
        }
      default:
        {
          PRINTENDFUNC;
          return -1;
        }
    }

  }

uint32_t HwEmShim::getAddressSpace (uint32_t topology)
{
  if(mMembanks.size() <= topology)
    return 0;
  if(mMembanks[topology].tag.find("bank") != std::string::npos)
  {
    return 0;
  }
  return 1;
}
  size_t HwEmShim::xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek, uint32_t topology)
  {
    if(!sock)
    {
      if(mMemModel)
        mMemModel = new mem_model(deviceName);
      mMemModel->writeDevMem(dest,src,size);
      return size;
    }
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
        << src << ", " << size << ", " << seek << std::endl;
    }
    std::string dMsg ="INFO: [SDx-EM 02-0] Copying buffer from host to device started : size = " + std::to_string(size);
    logMessage(dMsg,1);
    //_profile_inst->profile_buffer_size(size);
    //_profile_inst->profile_bus_transfer_bandwidth(size,20,100000000);
    void *handle = this;

    unsigned int messageSize = xclemulation::config::getInstance()->getPacketSize();
    unsigned int c_size = messageSize;
    unsigned int processed_bytes = 0;
    while(processed_bytes < size){
      if((size - processed_bytes) < messageSize){
        c_size = size - processed_bytes;
      }else{
        c_size = messageSize;
      }

      void* c_src = (((unsigned char*)(src)) + processed_bytes);
      uint64_t c_dest = dest + processed_bytes;
#ifndef _WINDOWS
      // TODO: Windows build support
      // *_RPC_CALL uses unix_socket
      uint32_t space = getAddressSpace(topology);
      xclCopyBufferHost2Device_RPC_CALL(xclCopyBufferHost2Device,handle,c_dest,c_src,c_size,seek,space);
#endif
      processed_bytes += c_size;
    }
    dMsg ="INFO: [SDx-EM 02-1] Copying buffer from host to device ended";
    logMessage(dMsg,1);

    PRINTENDFUNC;
    printMem(mGlobalInMemStream, 16 , dest , (void*)src, size );

    return size;
  }

  size_t HwEmShim::xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip, uint32_t topology)
  {
    if(!sock)
    {
      if(mMemModel)
        mMemModel = new mem_model(deviceName);
      mMemModel->readDevMem(src,dest,size);
      return size;
    }
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
        << src << ", " << size << ", " << skip << std::endl;
    }

    std::string dMsg ="INFO: [SDx-EM 05-0] Copying buffer from device to host started. size := " + std::to_string(size);
    logMessage(dMsg,1);
    void *handle = this;

    unsigned int messageSize = xclemulation::config::getInstance()->getPacketSize();
    unsigned int c_size = messageSize;
    unsigned int processed_bytes = 0;

    while(processed_bytes < size){
      if((size - processed_bytes) < messageSize){
        c_size = size - processed_bytes;
      }else{
        c_size = messageSize;
      }

      void* c_dest = (((unsigned char*)(dest)) + processed_bytes);
      uint64_t c_src = src + processed_bytes;
#ifndef _WINDOWS
      // TODO: Windows build support
      // *_RPC_CALL uses unix_socket
      uint32_t space = getAddressSpace(topology);
      xclCopyBufferDevice2Host_RPC_CALL(xclCopyBufferDevice2Host,handle,c_dest,c_src,c_size,skip,space);
#endif

      processed_bytes += c_size;
    }
    dMsg ="INFO: [SDx-EM 05-1] Copying buffer from device to host ended";
    logMessage(dMsg,1);
    PRINTENDFUNC;
    printMem(mGlobalOutMemStream, 16 , src , dest , size );

    return size;
  }

  uint64_t HwEmShim::xclAllocDeviceBuffer(size_t size) 
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << size << std::endl;
    }

    uint64_t origSize = size;
    if (size == 0)
      size = DDR_BUFFER_ALIGNMENT;

    unsigned int paddingFactor = xclemulation::config::getInstance()->getPaddingFactor();
    uint64_t result = xclemulation::MemoryManager::mNull;
    for (auto i : mDDRMemoryManager) {
      result = i->alloc(size,paddingFactor);
      if (result != xclemulation::MemoryManager::mNull)
        break;
    }
    
    uint64_t finalValidAddress = result+(paddingFactor*size);
    uint64_t finalSize = size+(2*paddingFactor*size);
    mAddrMap[finalValidAddress] = finalSize;
    bool ack = false;
    if(sock)
    {
      xclAllocDeviceBuffer_RPC_CALL(xclAllocDeviceBuffer,finalValidAddress,origSize);

      PRINTENDFUNC;
      if(!ack)
        return 0;
    }
    return finalValidAddress;
  }

  uint64_t HwEmShim::xclAllocDeviceBuffer2(size_t& size, xclMemoryDomains domain, unsigned flags)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << size <<", "<<domain<<", "<< flags <<std::endl;
    }

    //flags = flags % 32;
    if (domain != XCL_MEM_DEVICE_RAM)
    {
      PRINTENDFUNC;
      return xclemulation::MemoryManager::mNull;
    }

    if (size == 0)
      size = DDR_BUFFER_ALIGNMENT;

    if (flags >= mDDRMemoryManager.size()) {
      PRINTENDFUNC;
      return xclemulation::MemoryManager::mNull;
    }
    uint64_t origSize = size;
    unsigned int paddingFactor = xclemulation::config::getInstance()->getPaddingFactor();
    uint64_t result = mDDRMemoryManager[flags]->alloc(size,paddingFactor);
    uint64_t finalValidAddress = result+(paddingFactor*size);
    uint64_t finalSize = size+(2*paddingFactor*size);
    mAddrMap[finalValidAddress] = finalSize;
    bool ack = false;
    if(sock)
    {
      xclAllocDeviceBuffer_RPC_CALL(xclAllocDeviceBuffer,finalValidAddress,origSize);

      PRINTENDFUNC;
      if(!ack)
        return 0;
    }
    return finalValidAddress;
  }


  void HwEmShim::xclFreeDeviceBuffer(uint64_t buf) 
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << buf << std::endl;
    }

    for (auto i : mDDRMemoryManager) {
      if (buf < i->size()) {
        i->free(buf);
      }
    }
    PRINTENDFUNC;
  }
  void HwEmShim::logMessage(std::string& msg , int verbosity)
  {
    if( verbosity > xclemulation::config::getInstance()->getVerbosityLevel())
      return;

    if ( mDebugLogStream.is_open())
      mDebugLogStream << msg<<std::endl;
    if(xclemulation::config::getInstance()->isInfosToBePrintedOnConsole())
      std::cout<<msg<<std::endl;
  }
  
  void HwEmShim::saveWaveDataBase()
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

    // The following is evil--hardcoding. This name may change.
    // Is there a way we can determine the name from the directories or otherwise?
    std::string bdName("dr"); // Used to be opencldesign. This is new default.

    int i = 0;
    for(auto it :mBinaryDirectories)
    {
      std::string binaryDirectory = it.first;
      std::string fileName = it.second;
      char path[FILENAME_MAX];
      size_t size = MAXPATHLEN;
      GetCurrentDir(path,size);

      // Copy waveform database
      std::string extension = "wdb";
      struct stat statBuf;
      if ( stat(std::string(binaryDirectory+ "/msim").c_str(), &statBuf) == 0 )
      {
        extension = "wlf";
      }
      std::string wdbFileName = binaryDirectory + "/" + fileName + "."+extension;
      std::string destPath = "'" + std::string(path) + "/" + fileName +"." + extension + "'";
      systemUtil::makeSystemCall(wdbFileName, systemUtil::systemOperation::COPY,destPath);

      // Copy waveform config
      std::string wcfgFilePath= binaryDirectory + "/" + bdName + "_behav.wcfg";
      std::string destPath2 = "'" + std::string(path) + "/" + fileName + ".wcfg'";
      systemUtil::makeSystemCall(wcfgFilePath, systemUtil::systemOperation::COPY, destPath2);

      // Append to detailed kernel trace data mining results file
      std::string logFilePath= binaryDirectory + "/sdaccel_profile_kernels.csv";
      std::string destPath3 = "'" + std::string(path) + "/sdaccel_profile_kernels.csv'";
      systemUtil::makeSystemCall(logFilePath, systemUtil::systemOperation::APPEND, destPath3);
      xclemulation::copyLogsFromOneFileToAnother(logFilePath, mDebugLogStream);

      // Append to detailed kernel trace "timeline" file
      std::string traceFilePath = binaryDirectory + "/sdaccel_timeline_kernels.csv";
      std::string destPath4 = "'" + std::string(path) + "/sdaccel_timeline_kernels.csv'";
      systemUtil::makeSystemCall(traceFilePath, systemUtil::systemOperation::APPEND, destPath4);

      if (mLogStream.is_open())
        mLogStream << "appended " << logFilePath << " to " << destPath3 << std::endl;

      // Copy Simulation Log file
      std::string simulationLogFilePath= binaryDirectory + "/" + "simulate.log";
      std::string destPath5 = "'" + std::string(path) + "/" + fileName + "_simulate.log'";
      systemUtil::makeSystemCall(simulationLogFilePath, systemUtil::systemOperation::COPY, destPath5);

      // Copy proto inst file
      std::string protoFilePath= binaryDirectory + "/" + bdName + "_behav.protoinst";
      std::string destPath6 = "'" + std::string(path) + "/" + fileName + ".protoinst'";
      systemUtil::makeSystemCall(protoFilePath, systemUtil::systemOperation::COPY, destPath6);

      i++;
    }
    mBinaryDirectories.clear();
    PRINTENDFUNC;
  }

  void HwEmShim::xclClose()
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }
    if (!sock) 
    {
      if( xclemulation::config::getInstance()->isKeepRunDirEnabled() == false)
        systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::REMOVE);
      if(mMBSch && mCore)
      {
        mMBSch->fini_scheduler_thread();
        delete mCore;
        mCore = NULL;
        delete mMBSch;
        mMBSch = NULL;
      }
      PRINTENDFUNC;
      return;
    }

    resetProgram(false);

    int status = 0;
    xclemulation::LAUNCHWAVEFORM lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();
    if(( lWaveform == xclemulation::LAUNCHWAVEFORM::GUI || lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH) && xclemulation::config::getInstance()->isInfoSuppressed() == false)
    {
      std::string waitingMsg ="INFO: [SDx-EM 06-0] Waiting for the simulator process to exit";
      logMessage(waitingMsg);
    }

    bool simDontRun = xclemulation::config::getInstance()->isDontRun();
    if(!simDontRun)
      while (-1 == waitpid(0, &status, 0));

    if(( lWaveform == xclemulation::LAUNCHWAVEFORM::GUI || lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH) && xclemulation::config::getInstance()->isInfoSuppressed() == false)
    {
      std::string waitingMsg ="INFO: [SDx-EM 06-1] All the simulator processes exited successfully";
      logMessage(waitingMsg);
    }

    saveWaveDataBase();
    if( xclemulation::config::getInstance()->isKeepRunDirEnabled() == false)
      systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::REMOVE);
    google::protobuf::ShutdownProtobufLibrary();
    PRINTENDFUNC;
    //void *handle = this;
  }

  int HwEmShim::resetProgram(bool saveWdb)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }
    if(!sock)
    {
      PRINTENDFUNC;
      if(mMBSch && mCore)
      {
        mMBSch->fini_scheduler_thread();
        delete mCore;
        mCore = NULL;
        delete mMBSch;
        mMBSch = NULL;
      }
      return 0;
    }

#ifndef _WINDOWS
    // TODO: Windows build support
    // *_RPC_CALL uses unix_socket
#endif
    Event eventObj;
    uint32_t numSlots = getPerfMonNumberSlots(XCL_PERF_MON_MEMORY);
    bool ack = true;
    for(unsigned int counter = 0 ; counter < numSlots; counter++)
    {
      unsigned int samplessize = 0;
      if (counter == XPAR_SPM0_HOST_SLOT)
        continue;

      char slotname[128];
      getPerfMonSlotName(XCL_PERF_MON_MEMORY,counter,slotname,128);

      if (simulator_started == true)
      {
#ifndef _WINDOWS
        // TODO: Windows build support
        // *_RPC_CALL uses unix_socket
        do 
        {
          bool accel=false;
          xclPerfMonReadTrace_RPC_CALL(xclPerfMonReadTrace,ack,samplessize,slotname,accel);
#endif
          for(unsigned int i = 0; i<samplessize ; i++)
          {
#ifndef _WINDOWS
            // TODO: Windows build support
            // r_msg is defined as part of *RPC_CALL definition
            const xclPerfMonReadTrace_response::events &event = r_msg.output_data(i);
            eventObj.timestamp = event.timestamp();
            eventObj.eventflags = event.eventflags();
            eventObj.arlen = event.arlen();
            eventObj.awlen = event.awlen();
            eventObj.host_timestamp = event.host_timestamp();
            eventObj.readBytes = event.rd_bytes();
            eventObj.writeBytes = event.wr_bytes();
            list_of_events[counter].push_back(eventObj);
#endif
          }
        } while (samplessize != 0);
      }
    }

    xclGetDebugMessages(true);
    simulator_started = false;
    std::string socketName = sock->get_name();
    if(socketName.empty() == false)// device is active if socketName is non-empty
    {
#ifndef _WINDOWS
      xclClose_RPC_CALL(xclClose,this);
#endif
      //clean up directories which are created inside the driver
      systemUtil::makeSystemCall(socketName, systemUtil::systemOperation::REMOVE);
    }

    if(saveWdb)
    {
      int status = 0;
      xclemulation::LAUNCHWAVEFORM lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();
      if(( lWaveform == xclemulation::LAUNCHWAVEFORM::GUI || lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH) && xclemulation::config::getInstance()->isInfoSuppressed() == false)
      {
        std::string waitingMsg ="INFO: [SDx-EM 06-0] Waiting for the simulator process to exit";
        logMessage(waitingMsg);
      }

      bool simDontRun = xclemulation::config::getInstance()->isDontRun();
      if(!simDontRun)
        while (-1 == waitpid(0, &status, 0));

      if(( lWaveform == xclemulation::LAUNCHWAVEFORM::GUI || lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH) && xclemulation::config::getInstance()->isInfoSuppressed() == false)
      {
        std::string waitingMsg ="INFO: [SDx-EM 06-1] All the simulator processes exited successfully";
        logMessage(waitingMsg);
      }

      saveWaveDataBase();
    }
    //ProfilerStop();
    delete sock;
    sock = NULL;
    PRINTENDFUNC;
    if(mMBSch && mCore)
    {
      mMBSch->fini_scheduler_thread();
      delete mCore;
      mCore = NULL;
      delete mMBSch;
      mMBSch = NULL;
    }

    return 0;
  }

  HwEmShim *HwEmShim::handleCheck(void *handle) {
    //TOOD: Need to find out what kidn of checks would be done here.
    // Sanity checks
    if (!handle)
      return 0;

    //Copying the pointer locally
    return (HwEmShim *)handle;

  }

  HwEmShim::~HwEmShim() {
    free(ci_buf);
    free(ri_buf);
    free(buf);
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
      mLogStream.close();
    }
    if (xclemulation::config::getInstance()->isMemLogsEnabled())
    {
      mGlobalInMemStream.close();
      mGlobalOutMemStream.close();
    }
    for(auto controlStreamItr : mOffsetInstanceStreamMap)
    {
      std::ofstream* os = controlStreamItr.second;
      if(os)
      {
        os->close();
        delete os;
        os=NULL;
      }
    }
    if(mMBSch && mCore)
    {
      mMBSch->fini_scheduler_thread();
      delete mCore;
      mCore = NULL;
      delete mMBSch;
      mMBSch = NULL;
    }
  }

  void HwEmShim::initMemoryManager(std::list<xclemulation::DDRBank>& DDRBankList)
  {
    std::list<xclemulation::DDRBank>::iterator start = DDRBankList.begin();
    std::list<xclemulation::DDRBank>::iterator end = DDRBankList.end();
    uint64_t base = 0;
    for(;start != end; start++)
    {
      const uint64_t bankSize = (*start).ddrSize; 
      mDdrBanks.push_back(*start);
       //CR 966701: alignment to 4k (instead of mDeviceInfo.mDataAlignment)
      mDDRMemoryManager.push_back(new xclemulation::MemoryManager(bankSize, base , 4096));
      base += bankSize;
    }
  }

  void HwEmShim::fillDeviceInfo(xclDeviceInfo2* dest, xclDeviceInfo2* src)
  {
    std::strcpy(dest->mName, src->mName);
    dest->mMagic               =    src->mMagic ;
    dest->mHALMajorVersion    =    src->mHALMajorVersion;
    dest->mHALMinorVersion    =    src->mHALMinorVersion;
    dest->mVendorId           =    src->mVendorId;
    dest->mDeviceId           =    src->mDeviceId;
    dest->mSubsystemVendorId  =    src->mSubsystemVendorId;
    dest->mDeviceVersion      =    src->mDeviceVersion;
    dest->mDDRSize            =    src->mDDRSize;
    dest->mDataAlignment      =    src->mDataAlignment;
    dest->mDDRBankCount       =    src->mDDRBankCount;
    for(unsigned int i = 0; i < 4 ;i++)
      dest->mOCLFrequency[i]       =    src->mOCLFrequency[i];

  }

  HwEmShim::HwEmShim( unsigned int deviceIndex, xclDeviceInfo2 &info, std::list<xclemulation::DDRBank>& DDRBankList, bool _unified, bool _xpr)
    :mRAMSize(info.mDDRSize)
    ,mCoalesceThreshold(4)
    ,mDSAMajorVersion(DSA_MAJOR_VERSION)
    ,mDSAMinorVersion(DSA_MINOR_VERSION)
    ,mDeviceIndex(deviceIndex)
  {
    simulator_started = false;
    tracecount_calls = 0;

    ci_msg.set_size(0);
    ci_msg.set_xcl_api(0);
    ci_buf = malloc(ci_msg.ByteSize());
    ri_msg.set_size(0);
    ri_buf = malloc(ri_msg.ByteSize());

    buf = NULL;
    buf_size = 0;
    binaryCounter = 0;
    sock = NULL;

    deviceName = "device"+std::to_string(deviceIndex); 
    deviceDirectory = xclemulation::getRunDirectory() +"/" + std::to_string(getpid())+"/hw_em/"+deviceName;

    std::memset(&mDeviceInfo, 0, sizeof(xclDeviceInfo2));
    fillDeviceInfo(&mDeviceInfo,&info);
    initMemoryManager(DDRBankList);

    last_clk_time = clock();
    mCloseAll = false;
    mMemModel = NULL;

    // Delete detailed kernel trace data mining results file
    // NOTE: do this only if we're going to write a new one
    xclemulation::LAUNCHWAVEFORM lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();
    if (lWaveform == xclemulation::LAUNCHWAVEFORM::GUI
        || lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH) {
      char path[FILENAME_MAX];
      size_t size = MAXPATHLEN;
      GetCurrentDir(path,size);
      std::string sdxProfileKernelFile = std::string(path) + "/sdaccel_profile_kernels.csv";
      systemUtil::makeSystemCall(sdxProfileKernelFile, systemUtil::systemOperation::REMOVE);
      std::string sdxTraceKernelFile = std::string(path) + "/sdaccel_timeline_kernels.csv";
      systemUtil::makeSystemCall(sdxTraceKernelFile, systemUtil::systemOperation::REMOVE);
    }
    bUnified = _unified;
    bXPR = _xpr;
    mCore = NULL;
    mMBSch = NULL;
    mIsDebugIpLayoutRead = false;
    mIsDeviceProfiling = false;
    mMemoryProfilingNumberSlots = 0;
    mAccelProfilingNumberSlots = 0;
    mStallProfilingNumberSlots = 0;
    mPerfMonFifoCtrlBaseAddress = 0;
    mPerfMonFifoReadBaseAddress = 0;

  }

  void HwEmShim::xclReadBusStatus(xclPerfMonType type) {

    bool is_bus_idle = true;
    uint64_t l_idle_bus_cycles = 0;
    uint64_t idle_bus_cycles = 0;
    time_t currentTime;
    struct tm *localTime;

    time( &currentTime );
    localTime = localtime( &currentTime );
    std::string time_s = "[Time: " + std::to_string(localTime->tm_hour) + ":" + std::to_string(localTime->tm_min) + "]";

    for(uint32_t slot_n = 0; slot_n < getPerfMonNumberSlots(type)-1; slot_n++) {
      xclReadBusStatus_RPC_CALL(xclReadBusStatus,idle_bus_cycles,slot_n);

      is_bus_idle = is_bus_idle & (idle_bus_cycles > 0);
      if(idle_bus_cycles > 0) {
        l_idle_bus_cycles = idle_bus_cycles;
      }
    }

    if(is_bus_idle) {
      std::cout << "INFO " << time_s <<" There is no traffic between DDR Memory and Kernel for last " << l_idle_bus_cycles << " clock cycles" << std::endl;
    } else {
      if ((clock() - last_clk_time)/CLOCKS_PER_SEC >  60*5) {
        last_clk_time = clock();
        std::cout << "INFO " << time_s<<" Hardware Emulation is in progress..." << std::endl;
      }
    }
  }

  void HwEmShim::xclGetDebugMessages(bool force)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

    bool ack = true;
    std::string displayMsgs;
    std::string logMsgs;
    std::string stopMsgs;
    xclGetDebugMessages_RPC_CALL(xclGetDebugMessages,ack,force,displayMsgs,logMsgs,stopMsgs);
    //std::cout<<"display msgs are "<<displayMsgs<<std::endl;
    //
    //as of now, we dont know whether file is already opened or not
    if(mDebugLogStream.is_open() && logMsgs.empty() == false)
    {
      mDebugLogStream <<logMsgs;
      mDebugLogStream.flush();
    }
    if(displayMsgs.empty() == false)
    {
      std::cout<<displayMsgs;
      std::cout.flush();
    }
    PRINTENDFUNC;
    //std::cout<<"stop msgs are "<<stopMsgs<<std::endl;
  }

  size_t HwEmShim::xclReadSkipCopy(uint64_t offset, void *hostBuf, size_t size)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", "
        << offset << ", " << hostBuf << ", " << size << std::endl;
    }

    const size_t mod_size = offset % DDR_BUFFER_ALIGNMENT;
    // Need to do Read-Modify-Read
#ifndef _WINDOWS
    // TODO: Windows build support
    //    alignas is defined in c++11
    alignas(DDR_BUFFER_ALIGNMENT) char buffer[DDR_BUFFER_ALIGNMENT];
#else
    char buffer[DDR_BUFFER_ALIGNMENT];
#endif

    // Read back one full aligned block starting from preceding aligned address
    const uint64_t mod_offset = offset - mod_size;
    if (xclRead(XCL_ADDR_SPACE_DEVICE_RAM, mod_offset, buffer, DDR_BUFFER_ALIGNMENT) != DDR_BUFFER_ALIGNMENT)
    {
      PRINTENDFUNC;
      return -1;
    }

    const size_t copy_size = (size + mod_size > DDR_BUFFER_ALIGNMENT) ? DDR_BUFFER_ALIGNMENT - mod_size : size;

    // Update the user buffer with partial read
    std::memcpy(hostBuf, buffer + mod_size, copy_size);

    // Update the remainder of user buffer
    if (size + mod_size > DDR_BUFFER_ALIGNMENT) {
      const size_t read_size = xclRead(XCL_ADDR_SPACE_DEVICE_RAM, mod_offset + DDR_BUFFER_ALIGNMENT,
          (char *)hostBuf + copy_size, size - copy_size);
      if (read_size != (size - copy_size))
      {
        PRINTENDFUNC;
        return -1;
      }
    }
    PRINTENDFUNC;
    return size;
  }

  size_t HwEmShim::xclReadModifyWrite(uint64_t offset, const void *hostBuf, size_t size)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", "
        << offset << ", " << hostBuf << ", " << size << std::endl;
    }

#ifndef _WINDOWS
    // TODO: Windows build support
    //    alignas is defined in c++11
    alignas(DDR_BUFFER_ALIGNMENT) char buffer[DDR_BUFFER_ALIGNMENT];
#else
    char buffer[DDR_BUFFER_ALIGNMENT];
#endif

    const size_t mod_size = offset % DDR_BUFFER_ALIGNMENT;
    // Read back one full aligned block starting from preceding aligned address
    const uint64_t mod_offset = offset - mod_size;
    if (xclRead(XCL_ADDR_SPACE_DEVICE_RAM, mod_offset, buffer, DDR_BUFFER_ALIGNMENT) != DDR_BUFFER_ALIGNMENT)
    {
      PRINTENDFUNC;
      return -1;
    }

    // Update the local copy of buffer with user requested data
    const size_t copy_size = (size + mod_size > DDR_BUFFER_ALIGNMENT) ? DDR_BUFFER_ALIGNMENT - mod_size : size;
    std::memcpy(buffer + mod_size, hostBuf, copy_size);

    // Write back the updated aligned block
    if (xclWrite(XCL_ADDR_SPACE_DEVICE_RAM, mod_offset, buffer, DDR_BUFFER_ALIGNMENT) != DDR_BUFFER_ALIGNMENT)
    {
      PRINTENDFUNC;
      return -1;
    }

    // Write any remaining blocks over DDR_BUFFER_ALIGNMENT size
    if (size + mod_size > DDR_BUFFER_ALIGNMENT) {
      size_t write_size = xclWrite(XCL_ADDR_SPACE_DEVICE_RAM, mod_offset + DDR_BUFFER_ALIGNMENT,
          (const char *)hostBuf + copy_size, size - copy_size);
      if (write_size != (size - copy_size))
      {
        PRINTENDFUNC;
        return -1;
      }
    }
    PRINTENDFUNC;
    return size;
  }

  int HwEmShim::xclGetDeviceInfo2(xclDeviceInfo2 *info)
  {
    std::memset(info, 0, sizeof(xclDeviceInfo2));
    fillDeviceInfo(info,&mDeviceInfo);
    for (auto i : mDDRMemoryManager) {
      info->mDDRFreeSize += i->freeSize();
    }
    return 0;
  }

  //TODO::SPECIFIC TO LINUX
  //Need to modify for windows
  void HwEmShim::xclOpen(const char* logfileName)
  {
    //populate environment information in driver
    xclemulation::config::getInstance()->populateEnvironmentSetup(mEnvironmentNameValueMap);
    char path[FILENAME_MAX];
    size_t size = MAXPATHLEN;
    GetCurrentDir(path,size);
    std::string sdxProfileKernelFile = std::string(path) + "/sdaccel_profile_kernels.csv";
    systemUtil::makeSystemCall(sdxProfileKernelFile, systemUtil::systemOperation::REMOVE);
    std::string sdxTraceKernelFile = std::string(path) + "/sdaccel_timeline_kernels.csv";
    systemUtil::makeSystemCall(sdxTraceKernelFile, systemUtil::systemOperation::REMOVE);
    if ( logfileName && (logfileName[0] != '\0')) 
    {
      mLogStream.open(logfileName);
      mLogStream << "FUNCTION, THREAD ID, ARG..."  << std::endl;
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }
    
    if (xclemulation::config::getInstance()->isMemLogsEnabled())
    {
      mGlobalInMemStream.open("global_in.mem");
      mGlobalOutMemStream.open("global_out.mem");
    }
  }

/**********************************************HAL2 API's START HERE **********************************************/

/*********************************** Utility ******************************************/

static int check_bo_user_flags(HwEmShim* dev, unsigned flags)
{
	const unsigned ddr_count = dev->xocl_ddr_channel_count();
	unsigned ddr;

	if(ddr_count == 0)
		return -EINVAL;
	if (flags == 0xffffffff)
		return 0;
	
  ddr = xocl_bo_ddr_idx(flags);
	if (ddr == 0xffffffff)
		return 0;
	
  if (ddr > ddr_count)
		return -EINVAL;
	
	return 0;
}

drm_xocl_bo* HwEmShim::xclGetBoByHandle(unsigned int boHandle)
{
  auto it = mXoclObjMap.find(boHandle);
  if(it == mXoclObjMap.end())
    return nullptr;

  drm_xocl_bo* bo = (*it).second;
  return bo;
}

inline unsigned short HwEmShim::xocl_ddr_channel_count()
{
  if(mMembanks.size() > 0)
    return mMembanks.size();
  return mDeviceInfo.mDDRBankCount;
}

inline unsigned long long HwEmShim::xocl_ddr_channel_size()
{
  return 0;
}

int HwEmShim::xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties)
{
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << std::endl;
  }
  drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if (!bo) {
    PRINTENDFUNC;
    return  -1;
  }
  properties->handle = bo->handle;
  properties->flags  = bo->flags;
  properties->size   = bo->size;
  properties->paddr  = bo->base;
  properties->domain = XCL_BO_DEVICE_RAM; // currently all BO domains are XCL_BO_DEVICE_RAM
  PRINTENDFUNC;
  return 0;
}
/*****************************************************************************************/

/******************************** xclAllocBO *********************************************/
int HwEmShim::xoclCreateBo(xocl_create_bo* info)
{
	size_t size = info->size;
  unsigned ddr = xocl_bo_ddr_idx(info->flags);

  if (!size)
  {
    return -1;
  }

  /* Either none or only one DDR should be specified */
  if (check_bo_user_flags(this, info->flags))
  {
    return -1;
  }
	
  struct drm_xocl_bo *xobj = new drm_xocl_bo;

  xobj->base = xclAllocDeviceBuffer2(size,XCL_MEM_DEVICE_RAM,ddr);
  xobj->size = size;
  xobj->flags=info->flags;
  xobj->userptr = NULL;
  xobj->buf = NULL;
  xobj->topology=ddr;

  info->handle = mBufferCount;
  mXoclObjMap[mBufferCount++] = xobj;
  return 0;
}

unsigned int HwEmShim::xclAllocBO(size_t size, xclBOKind domain, unsigned flags)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << size << std::dec << " , "<<domain <<" , "<< flags<< std::endl;
  }
  xocl_create_bo info = {size, mNullBO, flags};
  int result = xoclCreateBo(&info);
  PRINTENDFUNC;
  return result ? mNullBO : info.handle;
}
/***************************************************************************************/

/******************************** xclAllocUserPtrBO ************************************/
unsigned int HwEmShim::xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << userptr <<", " << std::hex << size << std::dec <<" , "<< flags<< std::endl;
  }
  xocl_create_bo info = {size, mNullBO, flags};
  int result = xoclCreateBo(&info);
  drm_xocl_bo* bo = xclGetBoByHandle(info.handle);
  if (bo) {
    bo->userptr = userptr;
  }
  PRINTENDFUNC;
  return result ? mNullBO : info.handle;
}
/***************************************************************************************/

/******************************** xclExportBO *******************************************/
int HwEmShim::xclExportBO(unsigned int boHandle)
{
  //TODO
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << std::endl;
  }
  PRINTENDFUNC;
  return 0;
}
/***************************************************************************************/

/******************************** xclImportBO *******************************************/
unsigned int HwEmShim::xclImportBO(int boGlobalHandle)
{
  //TODO
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boGlobalHandle << std::endl;
  }
  PRINTENDFUNC;
  return 0;
}
/***************************************************************************************/

/******************************** xclMapBO *********************************************/
void *HwEmShim::xclMapBO(unsigned int boHandle, bool write)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << " , " << write << std::endl;
  }
  drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if (!bo) {
    PRINTENDFUNC;
    return nullptr;
  }

  void *pBuf=nullptr;
  if (posix_memalign(&pBuf, sizeof(double)*16, bo->size)) 
  {
    if (mLogStream.is_open()) mLogStream << "posix_memalign failed" << std::endl;
    pBuf=nullptr;
  }
  memset(pBuf, 0, bo->size);
  bo->buf = pBuf;
  PRINTENDFUNC;
  return pBuf;
}

/**************************************************************************************/

/******************************** xclSyncBO *******************************************/
int HwEmShim::xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  std::lock_guard<std::mutex> lk(mApiMtx);

  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << " , " << std::endl;
  }
  drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if(!bo)
  {
    PRINTENDFUNC;
    return -1;
  }

  int returnVal = -1;
  if(dir == XCL_BO_SYNC_BO_TO_DEVICE)
  {
    void* buffer =  bo->userptr ? bo->userptr : bo->buf;
    returnVal = xclCopyBufferHost2Device(bo->base,buffer, size,0, bo->topology);
  }
  else
  {
    void* buffer =  bo->userptr ? bo->userptr : bo->buf;
    returnVal = xclCopyBufferDevice2Host(buffer, bo->base, size,0, bo->topology);
  }
  PRINTENDFUNC;
  return returnVal;
}
/***************************************************************************************/

/******************************** xclFreeBO *******************************************/
void HwEmShim::xclFreeBO(unsigned int boHandle)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << std::endl;
  }
  auto it = mXoclObjMap.find(boHandle);
  if(it == mXoclObjMap.end())
  {
    PRINTENDFUNC;
    return;
  }
  drm_xocl_bo* bo = (*it).second;;
  xclFreeDeviceBuffer(bo->base);
  mXoclObjMap.erase(it);
  PRINTENDFUNC;
}
/***************************************************************************************/

/******************************** xclWriteBO *******************************************/
size_t HwEmShim::xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << " , "<< src <<" , "<< size << ", " << seek << std::endl;
  }
  drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if(!bo)
  {
    PRINTENDFUNC;
    return -1;
  }
  int returnVal = xclCopyBufferHost2Device( bo->base, src, size,seek,bo->topology);
  PRINTENDFUNC;
  return returnVal;
}
/***************************************************************************************/

/******************************** xclReadBO *******************************************/
size_t HwEmShim::xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << " , "<< dst <<" , "<< size << ", " << skip << std::endl;
  }
  drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if(!bo)
  {
    PRINTENDFUNC;
    return -1;
  }
  int returnVal = xclCopyBufferDevice2Host(dst, bo->base, size, skip, bo->topology);
  PRINTENDFUNC;
  return returnVal;
}
/***************************************************************************************/

int HwEmShim::xclExecBuf(unsigned int cmdBO)
{
  
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << cmdBO << std::endl;
  }
  drm_xocl_bo* bo = xclGetBoByHandle(cmdBO);
  if(!mMBSch || !bo)
  {
    PRINTENDFUNC;
    return -1;
  }
  mMBSch->add_exec_buffer(mCore, bo);
  PRINTENDFUNC;
  return 0;
}

int HwEmShim::xclRegisterEventNotify(unsigned int userInterrupt, int fd)
{
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << userInterrupt <<", "<< fd << std::endl;
  }
  PRINTENDFUNC;
  return 0;
}

int HwEmShim::xclExecWait(int timeoutMilliSec)
{
  if (mLogStream.is_open()) 
  {
 //   mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << timeoutMilliSec << std::endl;
  }

  static bool configurationWait = true;
  unsigned int tSec = 0;
  static bool bConfig = true;
  tSec = timeoutMilliSec/1000;
  if(bConfig)
  {
    tSec = timeoutMilliSec/100;
    bConfig = false;
  }
  sleep(tSec);
  //PRINTENDFUNC;
  return 1;
}

/**********************************************HAL2 API's END HERE **********************************************/

}  // end namespace xclhwemhal2



