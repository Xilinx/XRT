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
 * Copyright (C) 2015 Xilinx, Inc
 */

#include "shim.h"
namespace xclcpuemhal2 {

  std::map<unsigned int, CpuemShim*> devices;
  unsigned int CpuemShim::mBufferCount = 0;
  bool CpuemShim::mFirstBinary = true;
  const unsigned CpuemShim::TAG = 0X586C0C6C; // XL OpenCL X->58(ASCII), L->6C(ASCII), O->0 C->C L->6C(ASCII);
  const unsigned CpuemShim::CONTROL_AP_START = 1;
  const unsigned CpuemShim::CONTROL_AP_DONE  = 2;
  const unsigned CpuemShim::CONTROL_AP_IDLE  = 4;
  std::map<std::string, std::string> CpuemShim::mEnvironmentNameValueMap(xclemulation::getEnvironmentByReadingIni());
#define PRINTENDFUNC if (mLogStream.is_open()) mLogStream << __func__ << " ended " << std::endl;
 
  CpuemShim::CpuemShim(unsigned int deviceIndex, xclDeviceInfo2 &info, std::list<xclemulation::DDRBank>& DDRBankList, bool _unified, bool _xpr) 
    :mTag(TAG)
    ,mRAMSize(info.mDDRSize)
    ,mCoalesceThreshold(4)
    ,mDSAMajorVersion(DSA_MAJOR_VERSION)
    ,mDSAMinorVersion(DSA_MINOR_VERSION)
    ,mDeviceIndex(deviceIndex)
  {
    binaryCounter = 0;
    sock = NULL;
    ci_msg.set_size(0);
    ci_msg.set_xcl_api(0);

    ci_buf = malloc(ci_msg.ByteSize());
    ri_msg.set_size(0);
    ri_buf = malloc(ri_msg.ByteSize());
    buf = NULL;
    buf_size = 0;
    
    deviceName = "device"+std::to_string(deviceIndex); 
    deviceDirectory = xclemulation::getRunDirectory() + "/"+std::to_string(getpid())+"/cpu_em/"+deviceName;
    simulator_started = false;
    mVerbosity = XCL_INFO;

    std::memset(&mDeviceInfo, 0, sizeof(xclDeviceInfo2));
    fillDeviceInfo(&mDeviceInfo,&info);
    initMemoryManager(DDRBankList);

    char* pack_size = getenv("CPU_EM_PACKET_SIZE");
    if(pack_size)
    {
      unsigned int messageSize = strtoll(pack_size,NULL,0);
      message_size = messageSize;
    }
    else
    {
      message_size = 0x800000;
    }
    mCloseAll = false;
    bUnified = _unified;
    bXPR = _xpr;
  }
 
  size_t CpuemShim::alloc_void(size_t new_size) 
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

 
  void CpuemShim::initMemoryManager(std::list<xclemulation::DDRBank>& DDRBankList)
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

//private 
  bool CpuemShim::isGood() const 
  {
    // TODO: Add sanity check for card state
    return true;
  }

  CpuemShim *CpuemShim::handleCheck(void *handle) 
  {
    // Sanity checks
    if (!handle)
      return 0;
    if (*(unsigned *)handle != TAG)
      return 0;
    if (!((CpuemShim *)handle)->isGood()) {
      return 0;
    }
    return (CpuemShim *)handle;
  }

  static void saveDeviceProcessOutputs()
  {
    std::map<unsigned int, CpuemShim*>::iterator start = devices.begin();
    std::map<unsigned int, CpuemShim*>::iterator end = devices.end();
    for(; start != end; start++)
    {
      CpuemShim* handle = (*start).second;
      if(!handle)
        continue;
      handle->saveDeviceProcessOutput();
    }

  }
 
  static void sigHandler(int sn, siginfo_t *si, void *sc)
  {
    switch(sn) {
      case SIGSEGV:
        {
          saveDeviceProcessOutputs();
          kill(0,SIGSEGV);
          exit(1);
          break;
        }
      case SIGFPE :
        {
          saveDeviceProcessOutputs();
          kill(0,SIGTERM);
          exit(1);
          break;
        }
      case SIGABRT:
        {
          saveDeviceProcessOutputs();
          kill(0,SIGABRT);
          exit(1);
          break;
        }
    case SIGUSR1:
        {
	  // One of the spawned processes died for some reason,
	  //  kill all of the others and exit the host code
	  saveDeviceProcessOutputs() ;
	  std::cerr << "CPU emulation compute unit exited unexpectedly" 
		    << std::endl ;
	  kill(0, SIGTERM) ; 
	  exit(1) ;
	  break ;
        }
      default:
        {
          break;
        }
    }
  }

  int CpuemShim::dumpXML(const xclBin* header, std::string& fileLocation)
  {
    if (!header) return 0 ; // We didn't dump it, but this isn't an error

    char* xclbininmemory = 
      reinterpret_cast<char*>(const_cast<xclBin*>(header)) ;

    char* xmlfile = nullptr ;
    int xmllength = 0 ;

    if (memcmp(xclbininmemory, "xclbin0", 8) == 0)
    {
       if (mLogStream.is_open()) 
       {
	   mLogStream << __func__ << " unsupported Legacy XCLBIN header " << std::endl;
       }
       return -1;

      //xmlfile = xclbininmemory + (header->m_metadataOffset) ;
      //xmllength = (int)(header->m_metadataLength);       
    }
    else if (memcmp(xclbininmemory,"xclbin2",7) == 0) 
    {
      auto top = reinterpret_cast<const axlf*>(header);
      if (auto sec = xclbin::get_axlf_section(top,EMBEDDED_METADATA)) {
	xmlfile = xclbininmemory + sec->m_sectionOffset;
	xmllength = sec->m_sectionSize;
      }
    }
    else
    {
      // This was not a valid xclbin file
      if (mLogStream.is_open()) 
      {
	mLogStream << __func__ << " invalid XCLBIN header " << std::endl;
      }
      return -1 ;
    }

    if (xmlfile == nullptr || xmllength == 0)
    {
      // This xclbin file did not contain any XML meta-data
      if (mLogStream.is_open())
      {
	mLogStream << __func__ << " XCLBIN did not contain meta-data" 
		   << std::endl ;
      }
      return -1 ;
    }

    // First, create the device directory if it doesn't exist
    systemUtil::makeSystemCall(deviceDirectory,
			       systemUtil::systemOperation::CREATE) ;
    // Second, create the binary directory if it doesn't exist
    std::stringstream binaryDirectory ;
    binaryDirectory << deviceDirectory << "/binary_" << binaryCounter ;
    std::string binDir = binaryDirectory.str() ;
    systemUtil::makeSystemCall(binDir,
			       systemUtil::systemOperation::CREATE) ;
    systemUtil::makeSystemCall(binDir, 
			       systemUtil::systemOperation::PERMISSIONS,
			       "777") ;

    // The XML file will exist in this binary directory
    fileLocation = binDir + "/xmltmp" ;

    // Keep appending underscore to the file name until we find
    //  a file that does not exist.
    bool foundName = false ;
    while (!foundName)
    {
      FILE* fp = fopen(fileLocation.c_str(), "rb") ;
      if (fp == NULL)
      {
	// The file does not exist, so we can use this file location
	foundName = true ;
      }
      else
      {
	// The name we've chosen already exists, so append an underscore
	//  and try again
	fclose(fp) ;
	fileLocation += "_" ;
      }
    }

    // The file name we've chosen does not exist, so attempt to 
    //  open it for writing 
    FILE* fp = fopen(fileLocation.c_str(), "wb") ;
    if(fp==NULL)
    {
      if (mLogStream.is_open()) 
      {
	mLogStream << __func__ << " failed to create temporary xml file " << std::endl;
      }
      return -1;
    }
    fwrite(xmlfile,xmllength,1,fp);
    fflush(fp);
    fclose(fp);

    return 0 ;
  }

  bool CpuemShim::parseIni(unsigned int& debugPort)
  {
    debugPort = xclemulation::config::getInstance()->getServerPort() ;
    if (debugPort == 0)
    {
      return false ;
    }
    return true ;
  }
  
  void CpuemShim::launchDeviceProcess(bool debuggable, std::string& binaryDirectory)
  {
    std::lock_guard<std::mutex> lk(mProcessLaunchMtx);
    systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::CREATE);
    std::stringstream ss1;
    ss1<<deviceDirectory<<"/binary_"<<binaryCounter;
    binaryDirectory = ss1.str();
    systemUtil::makeSystemCall(binaryDirectory, systemUtil::systemOperation::CREATE);
    systemUtil::makeSystemCall(binaryDirectory, systemUtil::systemOperation::PERMISSIONS, "777");
    binaryCounter++;
    if(sock)
    {
      return;
    }

    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_flags = SA_SIGINFO;
    s.sa_sigaction = sigHandler;
    if (sigaction(SIGSEGV, &s, (struct sigaction *)0) ||
        sigaction(SIGFPE , &s, (struct sigaction *)0) ||
        sigaction(SIGABRT, &s, (struct sigaction *)0) ||
        sigaction(SIGUSR1, &s, (struct sigaction *)0))
    {
      //debug_print("unable to support all signals");
    }

    // We also need to check the .ini file in order to determine
    //  if the dynamic port on the sdx_server the child process
    //  must connect to was specified
    unsigned int debugPort = 0 ;
    bool passPort = parseIni(debugPort) ;
    std::stringstream portStream ;
    portStream << debugPort ;

    // If debuggable, the child process also requires the PID of the parent (us)
    pid_t parentPid = getpid() ;
    std::stringstream pidStream ;
    pidStream << parentPid ;

    // Spawn off the process to run the stub
    bool simDontRun = xclemulation::config::getInstance()->isDontRun();
    if(!simDontRun)
    {
      std::stringstream socket_id;
      socket_id << deviceName << "_" << binaryCounter << "_" << getpid();
      setenv("EMULATION_SOCKETID",socket_id.str().c_str(),true);

      pid_t pid = fork();
      assert(pid >= 0);
      if (pid == 0)
      { 
        //I am child
        std::string childProcessPath("");
        std::string xilinxInstall("");
        char *installEnvvar = getenv("XILINX_SDX");
        if (installEnvvar != NULL)
        {
          xilinxInstall = std::string(installEnvvar);
        }
        else
        {
          installEnvvar = getenv("XILINX_OPENCL");
          if (installEnvvar != NULL)
          {
            xilinxInstall = std::string(installEnvvar);
          }
        }
        char *xilinxVivadoEnvvar = getenv("XILINX_VIVADO");
        if(xilinxVivadoEnvvar)
        {
          std::string sHlsBinDir = xilinxVivadoEnvvar;
          std::string sLdLibs("");
          std::string DS("/");
          std::string sPlatform("lnx64");
          char* sLdLib = getenv("LD_LIBRARY_PATH");
          if (sLdLib) 
            sLdLibs = std::string(sLdLib) + ":"; 
          sLdLibs += sHlsBinDir +  DS + sPlatform + DS + "tools" + DS + "fft_v9_1" + ":";
          sLdLibs += sHlsBinDir +  DS + sPlatform + DS + "tools" + DS + "fir_v7_0" + ":";
          sLdLibs += sHlsBinDir +  DS + sPlatform + DS + "tools" + DS + "fpo_v7_0" + ":";
          sLdLibs += sHlsBinDir +  DS + sPlatform + DS + "tools" + DS + "dds_v6_0" + ":";
          sLdLibs += sHlsBinDir +  DS + sPlatform + DS + "tools" + DS + "opencv"   + ":";
          sLdLibs += sHlsBinDir +  DS + sPlatform + DS + "lib"   + DS + "csim";
          setenv("LD_LIBRARY_PATH",sLdLibs.c_str(),true);
        }
        std::string modelDirectory("");
#if defined(RDIPF_aarch64)
        modelDirectory= xilinxInstall + "/data/emulation/unified/cpu_em/zynqu/model/genericpciemodel";
#else
        modelDirectory= xilinxInstall + "/data/emulation/unified/cpu_em/generic_pcie/model/genericpciemodel";
#endif

        const char* childArgv[6] = { NULL, NULL, NULL, NULL, NULL, NULL } ;
        childArgv[0] = modelDirectory.c_str() ;

        // If we determined this should be debuggable, pass the proper
        //  arguments to the process
        if (debuggable)
        {
          childArgv[1] = "-debug" ;
          childArgv[2] = "-ppid" ;
          childArgv[3] = pidStream.str().c_str() ;

          if (passPort)
          {
            childArgv[4] = "-port" ;
            childArgv[5] = portStream.str().c_str() ;
          }
        }
        int r = execl(modelDirectory.c_str(), childArgv[0], childArgv[1],
            childArgv[2], childArgv[3], childArgv[4], childArgv[5],
            NULL) ;

        //fclose (stdout);
        if(r == -1){std::cerr << "FATAL ERROR : child process did not launch" << std::endl; exit(1);}
        exit(0);
      }
    }
    sock = new unix_socket;
  }

  int CpuemShim::xclLoadXclBin(const xclBin *header)
  {
    if(mLogStream.is_open()) mLogStream << __func__ << " begin " << std::endl;

    std::string xmlFile = "" ;
    int result = dumpXML(header, xmlFile) ;
    if (result != 0) return result ;

    // Before we spawn off the child process, we must determine
    //  if the process will be debuggable or not.  We get that
    //  by checking to see if there is a DEBUG_DATA section in
    //  the xclbin file.  Note, this only works with xclbin2
    //  files.  Also, the GUI can overwrite this by setting an
    //  environment variable
    bool debuggable = false ;
    if (getenv("SDA_SKIP_KERNEL_DEBUG") == NULL ||
	strcmp("true", getenv("SDA_SKIP_KERNEL_DEBUG")) != 0) 
    {
      char* xclbininmemory = 
        reinterpret_cast<char*>(const_cast<xclBin*>(header)) ;
      if (!memcmp(xclbininmemory, "xclbin2", 7))
      {
        auto top = reinterpret_cast<const axlf*>(header) ;
        auto sec = xclbin::get_axlf_section(top, DEBUG_DATA) ;
        if (sec)
        {
          debuggable = true ;
        }
      }      
    }

    std::string binaryDirectory("");
    launchDeviceProcess(debuggable,binaryDirectory);

    if(header)
    {

    if( mFirstBinary )
    {
      mFirstBinary = false;
    }

      char *xclbininmemory = reinterpret_cast<char*> (const_cast<xclBin*> (header));

      //parse header
      char *sharedlib = nullptr;
      int sharedliblength = 0;

      //check header
      if (!memcmp(xclbininmemory, "xclbin0", 8)) 
      {
        if (mLogStream.is_open()) 
        {
          mLogStream << __func__ << " invalid XCLBIN header " << std::endl;
        }
        return -1;
      }
      else if (!memcmp(xclbininmemory,"xclbin2",7)) {
        auto top = reinterpret_cast<const axlf*>(header);
        if (auto sec = xclbin::get_axlf_section(top,BITSTREAM)) {
          sharedlib = xclbininmemory + sec->m_sectionOffset;
          sharedliblength = sec->m_sectionSize;
        }
      }
      else
      {
        if (mLogStream.is_open()) 
        {
          mLogStream << __func__ << " invalid XCLBIN header " << std::endl;
          mLogStream << __func__ << " header " << xclbininmemory[0] << xclbininmemory[1] << xclbininmemory[2] <<  xclbininmemory[3] <<
            xclbininmemory[4] << xclbininmemory[5] << std::endl;
        }
        return -1;
      }
      //write out shared library to file for consumption with dlopen
      std::string tempdlopenfilename = binaryDirectory+"/dltmp"; 
      {
        bool tempfilecreated = false;
        while( !tempfilecreated ) {
          FILE *fp = fopen(tempdlopenfilename.c_str(),"rb");
          if(fp==NULL)
          {
            tempfilecreated = true;
          }
          else 
          {
            fclose(fp);
            std::stringstream ss;
            int r = rand();
            r &= 0xf;
            ss << std::hex << r;
            tempdlopenfilename+=ss.str();
          }
        }
        FILE *fp = fopen(tempdlopenfilename.c_str(),"wb");
        if( !fp ) 
        {
          if(mLogStream.is_open()) mLogStream << __func__ << " failed to create temporary dlopen file" << std::endl;
          return -1;
        }
        fwrite(sharedlib,sharedliblength,1,fp);
        fflush(fp);
        fclose(fp);
      }
      bool ack = true;
      bool verbose = false;
      if(mLogStream.is_open())
        verbose = true;
      xclLoadBitstream_RPC_CALL(xclLoadBitstream,xmlFile,tempdlopenfilename,deviceDirectory,binaryDirectory,verbose);
      if(!ack)
        return -1;
    }
    return 0;
  }

  int CpuemShim::xclGetDeviceInfo2(xclDeviceInfo2 *info) 
  {
    std::memset(info, 0, sizeof(xclDeviceInfo2));
    fillDeviceInfo(info,&mDeviceInfo);
    for (auto i : mDDRMemoryManager) 
    {
      info->mDDRFreeSize += i->freeSize();
    }
    return 0;
  }

  void CpuemShim::launchTempProcess()
  {
    std::string binaryDirectory("");
    launchDeviceProcess(false,binaryDirectory);
    std::string xmlFile("");
    std::string tempdlopenfilename("");
    SHIM_UNUSED bool ack = true;
    bool verbose = false;
    if(mLogStream.is_open())
      verbose = true;
    xclLoadBitstream_RPC_CALL(xclLoadBitstream,xmlFile,tempdlopenfilename,deviceDirectory,binaryDirectory,verbose);
  }

  uint64_t CpuemShim::xclAllocDeviceBuffer(size_t size) 
  {
    
    size_t requestedSize =  size; 
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << size << std::endl;
    }
    if(!sock)
    {
      launchTempProcess();
    }

    if (size == 0)
      size = DDR_BUFFER_ALIGNMENT;

    uint64_t result = xclemulation::MemoryManager::mNull;
    for (auto i : mDDRMemoryManager) {
      result = i->alloc(size);
      if (result != xclemulation::MemoryManager::mNull)
        break;
    }
    bool ack = false;
    //   Memory Manager Has allocated aligned address, 
	//   size contains alignement + original size requested.
	//   We are passing original size to device process for exact stats.	
	xclAllocDeviceBuffer_RPC_CALL(xclAllocDeviceBuffer,result,requestedSize);
    if(!ack)
    {
      PRINTENDFUNC;
      return 0;
    }
      PRINTENDFUNC;
    return result;
  }
  
  uint64_t CpuemShim::xclAllocDeviceBuffer2(size_t& size, xclMemoryDomains domain, unsigned flags)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ <<" , "<<std::this_thread::get_id() << ", " << size <<", "<<domain<<", "<< flags <<std::endl;
    }
    if(!sock)
    {
      launchTempProcess();
    }

    //flags = flags % 32;
    if (domain != XCL_MEM_DEVICE_RAM)
    {
      return xclemulation::MemoryManager::mNull;
    }

    if (size == 0)
      size = DDR_BUFFER_ALIGNMENT;

    if (flags >= mDDRMemoryManager.size()) 
    {
      return xclemulation::MemoryManager::mNull;
    }

    uint64_t result = mDDRMemoryManager[flags]->alloc(size);
    bool ack = false;
    //   Memory Manager Has allocated aligned address, 
	//   size contains alignement + original size requested.
	//   We are passing original size to device process for exact stats.	
    xclAllocDeviceBuffer_RPC_CALL(xclAllocDeviceBuffer,result,size);
    if(!ack)
    {
      PRINTENDFUNC;
      return 0;
    }
      PRINTENDFUNC;
    return result;
  }

  void CpuemShim::xclFreeDeviceBuffer(uint64_t offset) 
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << offset << std::endl;
    }

    for (auto i : mDDRMemoryManager) {
      if (offset < i->size()) {
        i->free(offset);
      }
    }
    bool ack = true;
    if(sock)
    {
      xclFreeDeviceBuffer_RPC_CALL(xclFreeDeviceBuffer,offset);
    }
    if(!ack)
    {
      PRINTENDFUNC;
      return;
    }
    PRINTENDFUNC;
    return;
  }

  size_t CpuemShim::xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size) 
  {
    std::lock_guard<std::mutex> lk(mApiMtx);
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << offset<<", "<<hostBuf<<", "<< size<<std::endl;
    }

    if(!sock)
      return size;

    if(space != XCL_ADDR_KERNEL_CTRL)
    {
      if (mLogStream.is_open()) mLogStream << "xclWrite called with xclAddressSpace != XCL_ADDR_KERNEL_CTRL " << std::endl;
      return -1;
    }

    if(size%4)
    {
      if (mLogStream.is_open()) mLogStream << "xclWrite only supports 32-bit writes" << std::endl;
      return -1;
    }

    fflush(stdout);
    xclWriteAddrKernelCtrl_RPC_CALL(xclWriteAddrKernelCtrl,space,offset,hostBuf,size,kernelArgsInfo);
    PRINTENDFUNC;
    return size;
  }

  size_t CpuemShim::xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size) 
  {
    std::lock_guard<std::mutex> lk(mApiMtx);
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << space << ", "
        << offset << ", " << hostBuf << ", " << size << std::endl;
    }

    if(!sock)
    {
      PRINTENDFUNC;
      return size;
    }

    if(space != XCL_ADDR_KERNEL_CTRL)
    {
      if (mLogStream.is_open()) mLogStream << "xclWrite called with xclAddressSpace != XCL_ADDR_KERNEL_CTRL " << std::endl;
      PRINTENDFUNC;
      return -1;
    }
    if(size!=4)
    {
      if (mLogStream.is_open()) mLogStream << "xclWrite called with size != 4 " << std::endl;
      PRINTENDFUNC;
      return -1;
    }
    xclReadAddrKernelCtrl_RPC_CALL(xclReadAddrKernelCtrl,space,offset,hostBuf,size);
    PRINTENDFUNC;
    return size;

  }

  

  size_t CpuemShim::xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek) 
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
        << src << ", " << size << ", " << seek << std::endl;
    }
    
    if(!sock)
    {
      launchTempProcess();
    }
    dest += seek;

    void *handle = this;

    unsigned int messageSize = get_messagesize();
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
      uint32_t space =0;
      xclCopyBufferHost2Device_RPC_CALL(xclCopyBufferHost2Device,handle,c_dest,c_src,c_size,seek,space);
#endif
      processed_bytes += c_size;
    }
    return size;
  }


  size_t CpuemShim::xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip) 
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
        << src << ", " << size << ", " << skip << std::endl;
    }

    if(!sock)
    {
      launchTempProcess();
    }
    src += skip;
    void *handle = this;

    unsigned int messageSize = get_messagesize();
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
      uint32_t space =0;
      xclCopyBufferDevice2Host_RPC_CALL(xclCopyBufferDevice2Host,handle,c_dest,c_src,c_size,skip,space);
#endif

      processed_bytes += c_size;
    }
    return size;

  }

  void CpuemShim::xclOpen(const char* logfileName)
  {
    xclemulation::config::getInstance()->populateEnvironmentSetup(mEnvironmentNameValueMap);
    if( logfileName && (logfileName[0] != '\0')) 
    {
      mLogStream.open(logfileName);
      mLogStream << "FUNCTION, THREAD ID, ARG..."  << std::endl;
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }
  }

  void CpuemShim::fillDeviceInfo(xclDeviceInfo2* dest, xclDeviceInfo2* src)
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
 
  void CpuemShim::saveDeviceProcessOutput()
  {
    if(!sock)
      return;

    for(int i = binaryCounter-1; i >= 0; i--)
    {
      std::stringstream cpu_em_folder;
      cpu_em_folder <<deviceDirectory<<"/binary_"<<i;
      char path[FILENAME_MAX];
      size_t size = PATH_MAX;
      char* pPath = GetCurrentDir(path,size);
      if(pPath)
      {
        std::string debugFilePath = cpu_em_folder.str()+"/genericpcieoutput";
        std::string destPath = std::string(path) + "/genericpcieoutput_device"+ std::to_string(mDeviceIndex) + "_"+std::to_string(i);
        systemUtil::makeSystemCall(debugFilePath, systemUtil::systemOperation::COPY,destPath);
      }
    }

  }
  void CpuemShim::resetProgram(bool callingFromClose)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }
    if(!sock)
      return;
    
    std::string socketName = sock->get_name();
    if(socketName.empty() == false)// device is active if socketName is non-empty
    {
#ifndef _WINDOWS
      xclClose_RPC_CALL(xclClose,this);
#endif
    }
   saveDeviceProcessOutput(); 
  }
  
  void CpuemShim::xclClose()
  {
    std::lock_guard<std::mutex> lk(mApiMtx);
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }
    if(!sock)
    {
      if( xclemulation::config::getInstance()->isKeepRunDirEnabled() == false)
        systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::REMOVE);
      return;
    }
    mCloseAll = true; 
    std::string socketName = sock->get_name();
    if(socketName.empty() == false)// device is active if socketName is non-empty
    {
#ifndef _WINDOWS
      xclClose_RPC_CALL(xclClose,this);
#endif
    }
    mCloseAll = false; 

    int status = 0;
    bool simDontRun = xclemulation::config::getInstance()->isDontRun();
    if(!simDontRun)
      while (-1 == waitpid(0, &status, 0));
    
    systemUtil::makeSystemCall(socketName, systemUtil::systemOperation::REMOVE);
    delete sock;
    sock = NULL;
    //clean up directories which are created inside the driver
    if( xclemulation::config::getInstance()->isKeepRunDirEnabled() == false)
    {
      //TODO sleeping for some time sothat gdb releases the process and its contents
      sleep(5);
      systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::REMOVE);
    }
    google::protobuf::ShutdownProtobufLibrary();
  }


  
  
  CpuemShim::~CpuemShim() 
  {
    if (mLogStream.is_open()) 
    {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }
    free(ci_buf);
    free(ri_buf);
    free(buf);
    
    if (mLogStream.is_open()) 
    {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
      mLogStream.close();
    }

    if (mLogStream.is_open()) 
    {
      mLogStream.close();
    }
  }

  /**********************************************HAL2 API's START HERE **********************************************/

/*********************************** Utility ******************************************/

static int check_bo_user_flags(CpuemShim* dev, unsigned flags)
{
	const unsigned ddr_count = dev->xocl_ddr_channel_count();
	unsigned ddr;

	if(ddr_count == 0)
		return -EINVAL;
	if (flags == 0xffffffff)
		return 0;
	
  ddr = xclemulation::xocl_bo_ddr_idx(flags);
	if (ddr == 0xffffffff)
		return 0;
	
  if (ddr > ddr_count)
		return -EINVAL;
	
	return 0;
}

xclemulation::drm_xocl_bo* CpuemShim::xclGetBoByHandle(unsigned int boHandle)
{
  auto it = mXoclObjMap.find(boHandle);
  if(it == mXoclObjMap.end())
    return nullptr;

  xclemulation::drm_xocl_bo* bo = (*it).second;
  return bo;
}

inline unsigned short CpuemShim::xocl_ddr_channel_count()
{
  return mDeviceInfo.mDDRBankCount;
}

inline unsigned long long CpuemShim::xocl_ddr_channel_size()
{
  return 0;
}

int CpuemShim::xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << std::endl;
  }
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
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
int CpuemShim::xoclCreateBo(xclemulation::xocl_create_bo* info)
{
  size_t size = info->size;
  unsigned ddr = xclemulation::xocl_bo_ddr_idx(info->flags);

  if (!size)
    return -1;

  /* Either none or only one DDR should be specified */
  if (check_bo_user_flags(this, info->flags))
    return -1;

	struct xclemulation::drm_xocl_bo *xobj = new xclemulation::drm_xocl_bo;
  xobj->base = xclAllocDeviceBuffer2(size,XCL_MEM_DEVICE_RAM,ddr);
  xobj->size = size;
  xobj->flags = info->flags;
  xobj->userptr = NULL;
  xobj->buf = NULL;

  info->handle = mBufferCount;
  mXoclObjMap[mBufferCount++] = xobj;
  return 0;
}

unsigned int CpuemShim::xclAllocBO(size_t size, xclBOKind domain, uint64_t flags)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  unsigned flag = flags & 0xFFFFFFFFLL;
  unsigned type =  (unsigned)(flags >> 32);
  flag |= type;
  std::cout  << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << size << std::dec << " , "<<domain <<" , "<< flag<< std::endl;
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << size << std::dec << " , "<<domain <<" , "<< flag<< std::endl;
  }
  xclemulation::xocl_create_bo info = {size, mNullBO, flag};
  int result = xoclCreateBo(&info);
  PRINTENDFUNC;
  return result ? mNullBO : info.handle;
}
/***************************************************************************************/

/******************************** xclAllocUserPtrBO ************************************/
unsigned int CpuemShim::xclAllocUserPtrBO(void *userptr, size_t size, uint64_t flags)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  unsigned flag = flags & 0xFFFFFFFFLL;
  unsigned type =  (unsigned)(flags >> 32);
  flag |= type;
  std::cout  << __func__ << ", " << std::this_thread::get_id() << ", " << userptr <<", " << std::hex << size << std::dec <<" , "<< flag<< std::endl;
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << userptr <<", " << std::hex << size << std::dec <<" , "<< flag<< std::endl;
  }
  xclemulation::xocl_create_bo info = {size, mNullBO, flag};
  int result = xoclCreateBo(&info);
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(info.handle);
  if (bo) {
    bo->userptr = userptr;
  }
  PRINTENDFUNC;
  return result ? mNullBO : info.handle;
}
/***************************************************************************************/

/******************************** xclExportBO *******************************************/
int CpuemShim::xclExportBO(unsigned int boHandle)
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
unsigned int CpuemShim::xclImportBO(int boGlobalHandle)
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
void *CpuemShim::xclMapBO(unsigned int boHandle, bool write)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << " , " << write << std::endl;
  }
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if (!bo) {
    PRINTENDFUNC;
    return nullptr;
  }

  void *pBuf=nullptr;
  if (posix_memalign(&pBuf, 4096, bo->size)) 
  {
    if (mLogStream.is_open()) mLogStream << "posix_memalign failed" << std::endl;
    pBuf=nullptr;
  }
  bo->buf = pBuf;
  PRINTENDFUNC;
  return pBuf;
}

/**************************************************************************************/

/******************************** xclSyncBO *******************************************/
int CpuemShim::xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << " , " << std::endl;
  }
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if(!bo)
  {
    PRINTENDFUNC;
    return -1;
  }

  int returnVal = -1;
  if(dir == XCL_BO_SYNC_BO_TO_DEVICE)
  {
    void* buffer =  bo->userptr ? bo->userptr : bo->buf;
    returnVal = xclCopyBufferHost2Device(bo->base,buffer, size,0);
  }
  else
  {
    void* buffer =  bo->userptr ? bo->userptr : bo->buf;
    returnVal = xclCopyBufferDevice2Host(buffer, bo->base, size,0);
  }
  PRINTENDFUNC;
  return returnVal;
}
/***************************************************************************************/

/******************************** xclFreeBO *******************************************/
void CpuemShim::xclFreeBO(unsigned int boHandle)
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
  xclemulation::drm_xocl_bo* bo = (*it).second;;
  if(bo)
  {
    xclFreeDeviceBuffer(bo->base);
    mXoclObjMap.erase(it);
  }
  PRINTENDFUNC;
}
/***************************************************************************************/

/******************************** xclWriteBO *******************************************/
size_t CpuemShim::xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << " , "<< src <<" , "<< size << ", " << seek << std::endl;
  }
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if(!bo)
  {
    PRINTENDFUNC;
    return -1;
  }
  int returnVal = xclCopyBufferHost2Device( bo->base, src, size,seek);
  PRINTENDFUNC;
  return returnVal;
}
/***************************************************************************************/

/******************************** xclReadBO *******************************************/
size_t CpuemShim::xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boHandle << " , "<< dst <<" , "<< size << ", " << skip << std::endl;
  }
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if(!bo)
  {
    PRINTENDFUNC;
    return -1;
  }
  int returnVal = xclCopyBufferDevice2Host(dst, bo->base, size, skip);
  PRINTENDFUNC;
  return returnVal;
}
/***************************************************************************************/

/**********************************************HAL2 API's END HERE **********************************************/
}



