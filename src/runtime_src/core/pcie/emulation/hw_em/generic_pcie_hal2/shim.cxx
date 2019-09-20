/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

#include "shim.h"
#include <string.h>
#include <boost/property_tree/xml_parser.hpp>
#include <errno.h>
#include <unistd.h>

#include "xcl_perfmon_parameters.h"

namespace xclhwemhal2 {

  namespace pt = boost::property_tree;
  std::map<unsigned int, HwEmShim*> devices;
  std::map<std::string, std::string> HwEmShim::mEnvironmentNameValueMap(xclemulation::getEnvironmentByReadingIni());
  std::map<int, std::tuple<std::string,int,void*> > HwEmShim::mFdToFileNameMap;
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
  const unsigned HwEmShim::CONTROL_AP_CONTINUE  = 0x10;
  void messagesThread(xclhwemhal2::HwEmShim* inst);
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
  static size_t convert(const std::string& str)
  {
    return str.empty() ? 0 : std::stoul(str,0,0);
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
    std::ios_base::fmtflags f( os.flags() );
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
    os.flags( f );
  }

  bool HwEmShim::isUltraScale() const
  {
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
    ssize_t pdiSize = 0;
    ssize_t emuDataSize = 0;

    char* zipFile = nullptr;
    char* xmlFile = nullptr;
    char* debugFile = nullptr;
    char* memTopology = nullptr;
    char* pdi = nullptr;
    char* emuData = nullptr;

    if (std::memcmp(bitstreambin, "xclbin2", 7)) {
      PRINTENDFUNC;
      return -1;
    }

    auto top = reinterpret_cast<const axlf*>(header);
    if (auto sec = xclbin::get_axlf_section(top, EMBEDDED_METADATA)) {
      xmlFileSize = sec->m_sectionSize;
      xmlFile = new char[xmlFileSize];
      memcpy(xmlFile, bitstreambin + sec->m_sectionOffset, xmlFileSize);
    }
    if (auto sec = xclbin::get_axlf_section(top, BITSTREAM)) {
      zipFileSize = sec->m_sectionSize;
      zipFile = new char[zipFileSize];
      memcpy(zipFile, bitstreambin + sec->m_sectionOffset, zipFileSize);
    }
    if (auto sec = xclbin::get_axlf_section(top, DEBUG_IP_LAYOUT)) {
      debugFileSize = sec->m_sectionSize;
      debugFile = new char[debugFileSize];
      memcpy(debugFile, bitstreambin + sec->m_sectionOffset, debugFileSize);
    }
    if (auto sec = xclbin::get_axlf_section(top, MEM_TOPOLOGY)) {
      memTopologySize = sec->m_sectionSize;
      memTopology = new char[memTopologySize];
      memcpy(memTopology, bitstreambin + sec->m_sectionOffset, memTopologySize);
    }
    if (auto sec = xclbin::get_axlf_section(top, PDI)) {
      pdiSize = sec->m_sectionSize;
      pdi = new char[pdiSize];
      memcpy(pdi, bitstreambin + sec->m_sectionOffset, pdiSize);
    }
    if (auto sec = xclbin::get_axlf_section(top, EMULATION_DATA)) {
      emuDataSize = sec->m_sectionSize;
      emuData = new char[emuDataSize];
      memcpy(emuData, bitstreambin + sec->m_sectionOffset, emuDataSize);
    }

    if(!zipFile || !xmlFile)
    {
      //deallocate all allocated memories to fix memory leak
      if(zipFile)
      {
        delete[] zipFile;
        zipFile = nullptr;
      }

      if(debugFile)
      {
        delete[] debugFile;
        debugFile = nullptr;
      }

      if(xmlFile)
      {
        delete[] xmlFile;
        xmlFile = nullptr;
      }

      if(memTopology)
      {
        delete[] memTopology;
        memTopology = nullptr;
      }

      if (pdi) {
        delete[] pdi;
        pdi = nullptr;
      }

      if (emuData) {
        delete[] emuData;
        emuData = nullptr;
      }
      return -1;
    }

    bitStreamArg loadBitStreamArgs;
    loadBitStreamArgs.m_zipFile = zipFile;
    loadBitStreamArgs.m_zipFileSize = zipFileSize;
    loadBitStreamArgs.m_xmlfile = xmlFile;
    loadBitStreamArgs.m_xmlFileSize = xmlFileSize;
    loadBitStreamArgs.m_debugFile = debugFile;
    loadBitStreamArgs.m_debugFileSize = debugFileSize;
    loadBitStreamArgs.m_memTopology = memTopology;
    loadBitStreamArgs.m_memTopologySize = memTopologySize;
    loadBitStreamArgs.m_pdi = pdi;
    loadBitStreamArgs.m_pdiSize = pdiSize;
    loadBitStreamArgs.m_emuData = emuData;
    loadBitStreamArgs.m_emuDataSize = emuDataSize;

    int returnValue = xclLoadBitstreamWorker(loadBitStreamArgs);

    //mFirstBinary is a static member variable which becomes false once first binary gets loaded
    if(returnValue >=0 && mFirstBinary )
    {
      HwEmShim::mDebugLogStream.open(xclemulation::getEmDebugLogFile(),std::ofstream::out);
      if(xclemulation::config::getInstance()->isInfoSuppressed() == false)
      {
        std::string initMsg ="INFO: [HW-EM 01] Hardware emulation runs simulation underneath. Using a large data set will result in long simulation times. It is recommended that a small dataset is used for faster execution. The flow uses approximate models for DDR memory and interconnect and hence the performance data generated is approximate.";
        logMessage(initMsg);
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
    delete[] pdi;
    delete[] emuData;

    PRINTENDFUNC;
    return returnValue;
  }

  int HwEmShim::xclLoadBitstreamWorker(bitStreamArg args)
  {
    if (mLogStream.is_open()) {
      //    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << args.m_zipFile << std::endl;
    }

    //TBD the file read may slowdown things...whenever xclLoadBitStream hal API implementation changes, we also need to make changes.
    std::unique_ptr<char[]> fileName(new char[1024]);
#ifndef _WINDOWS
    // TODO: Windows build support
    //    getpid is defined in unistd.h
    std::sprintf(fileName.get(), "%s/tempFile_%d", deviceDirectory.c_str(), binaryCounter);
#endif

    if (mMemModel)
    {
      delete mMemModel;
      mMemModel = NULL;
    }
    if (sock)
    {
      resetProgram();
    }
    std::stringstream ss;
    ss << deviceDirectory << "/binary_" << binaryCounter;
    std::string binaryDirectory = ss.str();

    systemUtil::makeSystemCall(binaryDirectory, systemUtil::systemOperation::CREATE);

    mRunDeviceBinDir = binaryDirectory;

    std::ofstream os(fileName.get());
    os.write(args.m_zipFile, args.m_zipFileSize);
    os.close();

    struct sigaction s;
    memset(&s, 0, sizeof(s));
    s.sa_flags = SA_SIGINFO;
    s.sa_sigaction = sigHandler;
    if (sigaction(SIGSEGV, &s, (struct sigaction *)0) ||
      sigaction(SIGFPE, &s, (struct sigaction *)0) ||
      sigaction(SIGABRT, &s, (struct sigaction *)0))
    {
      //debug_print("unable to support all signals");
    }

    std::string sim_path("");
    std::string sim_file("launch_hw_emu.sh");

    // Write and read debug IP layout (for debug & profiling)
    // NOTE: for now, let's do this file based so we can debug
    std::string debugFileName = mRunDeviceBinDir + "/debug_ip_layout";
    FILE *fp2 = fopen(debugFileName.c_str(), "wb");
    if (fp2 == NULL) {
      if (mLogStream.is_open())
        mLogStream << __func__ << " failed to create temporary debug_ip_layout file " << std::endl;
      return -1;
    }

    if ((args.m_debugFile != nullptr) && (args.m_debugFileSize > 1))
      fwrite(args.m_debugFile, args.m_debugFileSize, 1, fp2);
    fflush(fp2);
    fclose(fp2);

    std::string pdiFileName = binaryDirectory + "/aie_pdi";

    if ((args.m_pdi != nullptr) && (args.m_pdiSize > 1))
    {
      FILE *fp2 = fopen(pdiFileName.c_str(), "wb");
      if (fp2 == NULL) {
        if (mLogStream.is_open())
          mLogStream << __func__ << " failed to create temporary aie_pdi file " << std::endl;
        return -1;
      }

      fwrite(args.m_pdi, args.m_pdiSize, 1, fp2);
      fflush(fp2);
      fclose(fp2);
    }

    std::unique_ptr<char[]> emuDataFileName(new char[1024]);
#ifndef _WINDOWS
    // TODO: Windows build support
    // getpid is defined in unistd.h
    std::sprintf(emuDataFileName.get(), "%s/emuDataFile_%d", binaryDirectory.c_str(), binaryCounter);
#endif

    if ((args.m_emuData != nullptr) && (args.m_emuDataSize > 1))
    {
      std::ofstream os(emuDataFileName.get());
      os.write(args.m_emuData, args.m_emuDataSize);
      os.close();

      std::string emuDataFilePath(emuDataFileName.get());
      systemUtil::makeSystemCall(emuDataFilePath, systemUtil::systemOperation::UNZIP, binaryDirectory);
    }

    readDebugIpLayout(debugFileName);

    const mem_topology* m_mem = (reinterpret_cast<const ::mem_topology*>(args.m_memTopology));
    if (m_mem)
    {
      mMembanks.clear();
      for (int32_t i = 0; i<m_mem->m_count; ++i)
      {
        if (m_mem->m_mem_data[i].m_type == MEM_TYPE::MEM_STREAMING)
          continue;
        std::string tag = reinterpret_cast<const char*>(m_mem->m_mem_data[i].m_tag);
        mMembanks.emplace_back(membank{ m_mem->m_mem_data[i].m_base_address, tag, m_mem->m_mem_data[i].m_size * 1024, i });
      }
      if (m_mem->m_count > 0)
      {
        mDDRMemoryManager.clear();
      }

      for (auto it : mMembanks)
      {
        //CR 966701: alignment to 4k (instead of mDeviceInfo.mDataAlignment)
        mDDRMemoryManager.push_back(new xclemulation::MemoryManager(it.size, it.base_addr, getpagesize()));
      }
    }

    pt::ptree xml_project;
    std::string sXmlFile;
    sXmlFile.assign(args.m_xmlfile, args.m_xmlFileSize);
    std::stringstream xml_stream;
    xml_stream << sXmlFile;
    pt::read_xml(xml_stream, xml_project);

    // iterate platforms
    int count = 0;
    for (auto& xml_platform : xml_project.get_child("project"))
    {
      if (xml_platform.first != "platform")
        continue;
      if (++count > 1)
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
      if (++count > 1)
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
      if (++count > 1)
      {
        //Give error and return from here
      }
    }

    std::vector<std::string> kernels;

    // iterate kernels
    for (auto& xml_kernel : xml_project.get_child("project.platform.device.core"))
    {
      if (xml_kernel.first != "kernel")
        continue;
      std::string kernelName = xml_kernel.second.get<std::string>("<xmlattr>.name");
      kernels.push_back(kernelName);

      for (auto& xml_kernel_info : xml_kernel.second)
      {
        std::map<uint64_t, KernelArg> kernelArgInfo;
        if (xml_kernel_info.first == "arg")
        {
          std::string name = xml_kernel_info.second.get<std::string>("<xmlattr>.name");
          std::string id = xml_kernel_info.second.get<std::string>("<xmlattr>.id");
          std::string port = xml_kernel_info.second.get<std::string>("<xmlattr>.port");
          uint64_t offset = convert(xml_kernel_info.second.get<std::string>("<xmlattr>.offset"));
          uint64_t size = convert(xml_kernel_info.second.get<std::string>("<xmlattr>.size"));
          KernelArg kArg;
          kArg.name = kernelName + ":" + name;
          kArg.size = size;
          kernelArgInfo[offset] = kArg;
        }
        if (xml_kernel_info.first == "instance")
        {
          std::string instanceName = xml_kernel_info.second.get<std::string>("<xmlattr>.name");
          for (auto& xml_remap : xml_kernel_info.second)
          {
            if (xml_remap.first != "addrRemap")
              continue;
            uint64_t base = convert(xml_remap.second.get<std::string>("<xmlattr>.base"));
            mCuBaseAddress = base & 0xFFFFFFFF00000000;
            mKernelOffsetArgsInfoMap[base] = kernelArgInfo;
            if (xclemulation::config::getInstance()->isMemLogsEnabled())
            {
              std::ofstream* controlStream = new std::ofstream;
              controlStream->open(instanceName + "_control.mem");
              mOffsetInstanceStreamMap[base] = controlStream;
            }
            break;
          }
        }
      }
    }

    std::string xclBinName = xml_project.get<std::string>("project.<xmlattr>.name", "");
    set_simulator_started(true);

    //Thread to fetch messages from Device to display on host
    if(mMessengerThreadStarted == false) {
      mMessengerThread = std::thread(xclhwemhal2::messagesThread,this);
      mMessengerThreadStarted = true;
    }

    bool simDontRun = xclemulation::config::getInstance()->isDontRun();
    std::string launcherArgs = xclemulation::config::getInstance()->getLauncherArgs();
    std::string wdbFileName("");
    // The following is evil--hardcoding. This name may change.
    // Is there a way we can determine the name from the directories or otherwise?
    std::string bdName("dr"); // Used to be opencldesign. This is new default.
    if (!simDontRun)
    {
      wdbFileName = std::string(mDeviceInfo.mName) + "-" + std::to_string(mDeviceIndex) + "-" + xclBinName;
      xclemulation::LAUNCHWAVEFORM lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();
      std::string userSpecifiedSimPath = xclemulation::config::getInstance()->getSimDir();
      if (userSpecifiedSimPath.empty())
      {
        std::string _sFilePath(fileName.get());
        systemUtil::makeSystemCall(_sFilePath, systemUtil::systemOperation::UNZIP, binaryDirectory);
        systemUtil::makeSystemCall(binaryDirectory, systemUtil::systemOperation::PERMISSIONS, "777");
      }

      if (lWaveform == xclemulation::LAUNCHWAVEFORM::GUI)
      {
        // NOTE: proto inst filename must match name in HPIKernelCompilerHwEmu.cpp
        std::string protoFileName = "./" + bdName + "_behav.protoinst";
        std::stringstream cmdLineOption;
        cmdLineOption << " -g --wdb " << wdbFileName << ".wdb"
          << " --protoinst " << protoFileName;

        launcherArgs = launcherArgs + cmdLineOption.str();
        sim_path = binaryDirectory + "/behav_waveform/xsim";
        std::string generatedWcfgFileName = sim_path + "/" + bdName + "_behav.wcfg";
        unsetenv("VITIS_LAUNCH_WAVEFORM_BATCH");
        setenv("VITIS_WAVEFORM", generatedWcfgFileName.c_str(), true);
      }

      if (lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH)
      {
        // NOTE: proto inst filename must match name in HPIKernelCompilerHwEmu.cpp
        std::string protoFileName = "./" + bdName + "_behav.protoinst";
        std::stringstream cmdLineOption;
        cmdLineOption << " --wdb " << wdbFileName << ".wdb"
          << " --protoinst " << protoFileName;

        launcherArgs = launcherArgs + cmdLineOption.str();
        sim_path = binaryDirectory + "/behav_waveform/xsim";
        std::string generatedWcfgFileName = sim_path + "/" + bdName + "_behav.wcfg";
        setenv("VITIS_LAUNCH_WAVEFORM_BATCH", "1", true);
        setenv("VITIS_WAVEFORM", generatedWcfgFileName.c_str(), true);
      }

      if (userSpecifiedSimPath.empty() == false)
      {
        sim_path = userSpecifiedSimPath;
      }
      else
      {
        if (sim_path.empty())
        {
          sim_path = binaryDirectory + "/behav_gdb/xsim";
        }
        if (boost::filesystem::exists(sim_path) == false)
        {
          std::string dMsg = "WARNING: [HW-EM 07] None of the kernels is compiled in debug mode. Compile kernels in debug mode to launch waveform";
          logMessage(dMsg, 0);
          sim_path = binaryDirectory + "/behav_gdb/xsim";
        }
      }
      std::stringstream socket_id;
      socket_id << deviceName << "_" << binaryCounter << "_";
#ifndef _WINDOWS
      // TODO: Windows build support
      //   getpid is defined in unistd.h
      //   setenv is defined in stdlib.h
      socket_id << getpid();
      setenv("EMULATION_SOCKETID", socket_id.str().c_str(), true);
#endif
      binaryCounter++;
    }
    if (deviceDirectory.empty() == false)
      setenv("EMULATION_RUN_DIR", deviceDirectory.c_str(), true);


    // Create waveform config file
    // NOTE: see corresponding wdb file in saveWaveDataBase
    if (wdbFileName.empty() == false)
    {
      setenv("SDX_QUESTA_WLF_FILENAME", std::string(wdbFileName + ".wlf").c_str(), true);
      mBinaryDirectories[sim_path] = wdbFileName;
    }

    //launch simulation
    if (!sim_path.empty()) {
#ifndef _WINDOWS
      // TODO: Windows build support
      //   pid_t, fork, chdir, execl is defined in unistd.h
      //   this environment variable is added to disable the systemc copyright message
      setenv("SYSTEMC_DISABLE_COPYRIGHT_MESSAGE", "1", true);
      pid_t pid = fork();
      assert(pid >= 0);
      if (pid == 0){ //I am child
        //Redirecting the XSIM log to a file
        FILE* nP = freopen("/dev/null", "w", stdout);
        if (!nP) { std::cerr << "FATAR ERROR : Unable to redirect simulation output " << std::endl; exit(1); }

        int rV = chdir(sim_path.c_str());
        if (rV == -1){ std::cerr << "FATAL ERROR : Unable to go to simulation directory " << std::endl; exit(1); }

        // If the sdx server port was specified in the .ini file,
        //  we need to pass this information to the spawned xsim process.
        if (xclemulation::config::getInstance()->getServerPort() != 0)
        {
          std::stringstream convert;
          convert << xclemulation::config::getInstance()->getServerPort();
          setenv("XILINX_SDX_SERVER_PORT", convert.str().c_str(), 1);
        }

        if (mLogStream.is_open() && launcherArgs.empty() == false)
          mLogStream << __func__ << " xocc command line: " << launcherArgs << std::endl;

        const char* simMode = NULL;
        if (args.m_emuData) {
          //So far assuming that we will have only one AIE Kernel, need to 
          //update this logic when we have suport for multiple AIE Kernels
          launcherArgs += " -emuData " + binaryDirectory + "/" + kernels.at(0) + "/aieshim_solution.aiesol";
          launcherArgs += " -emu-data " + binaryDirectory + "/" + kernels.at(0) + "/aieshim_solution.aiesol";
          launcherArgs += " -bootBH " + binaryDirectory + "/" + kernels.at(0) + "/boot_bh.bin";
          launcherArgs += " -boot-bh " + binaryDirectory + "/" + kernels.at(0) + "/boot_bh.bin";
          launcherArgs += " -image " + binaryDirectory + "/" + kernels.at(0) + "/qemu_qspi.bin";
        }

        if (!launcherArgs.empty())
          simMode = launcherArgs.c_str();

        struct stat statBuf;
        if (stat(sim_file.c_str(), &statBuf) == -1)
        {
          sim_file = "simulate.sh";
        }
        int r = execl(sim_file.c_str(), sim_file.c_str(), simMode, NULL);
        fclose(stdout);
        if (r == -1){ std::cerr << "FATAL ERROR : Simulation process did not launch" << std::endl; exit(1); }
        exit(0);
      }
#endif
    }
    //if platform is a XPR platform, dont serilize ddr memory
    if (isXPR())
    {
      mEnvironmentNameValueMap["enable_pr"] = "false";
    }
    sock = new unix_socket;
    if (sock && mEnvironmentNameValueMap.empty() == false)
    {
      //send environment information to device
      bool ack = true;
      xclSetEnvironment_RPC_CALL(xclSetEnvironment);
      if (!ack)
      {
        //std::cout<<"environment is not set properly"<<std::endl;
      }
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
     offset = offset | mCuBaseAddress;
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
           const char *curr = (const char *)hostBuf;
           std::map<uint64_t,std::pair<std::string,unsigned int>> offsetArgInfo;
           xclWriteAddrKernelCtrl_RPC_CALL(xclWriteAddrKernelCtrl ,space,offset,curr,size,offsetArgInfo);
           PRINTENDFUNC;
           return size;
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
             std::string dMsg ="INFO: [HW-EM 04-0] Sending start signal to the kernel " + kernelName;
             logMessage(dMsg,1);
           }
           else
           {
             std::string dMsg ="INFO: [HW-EM 03-0] Configuring registers for the kernel " + kernelName +" Started";
             logMessage(dMsg,1);
           }
           xclWriteAddrKernelCtrl_RPC_CALL(xclWriteAddrKernelCtrl,space,offset,hostBuf,size,offsetArgInfo);
           if(hostBuf32[0] & CONTROL_AP_START)
           {
             std::string dMsg ="INFO: [HW-EM 04-1] Kernel " + kernelName +" is Started";
             logMessage(dMsg,1);
           }
           else
           {
             std::string dMsg ="INFO: [HW-EM 03-1] Configuring registers for the kernel " + kernelName +" Ended";
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

    //if(tracecount_calls < xclemulation::config::getInstance()->getMaxTraceCount())
    //{
    //  tracecount_calls = tracecount_calls + 1;
    //  return 0;
    //}
    //tracecount_calls = 0;

    if (!simulator_started)
      return 0;

    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << space << ", "
        << offset << ", " << hostBuf << ", " << size << std::endl;
    }
    offset = offset | mCuBaseAddress;
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
          xclGetDebugMessages();
          xclReadAddrKernelCtrl_RPC_CALL(xclReadAddrKernelCtrl,space,offset,hostBuf,size);
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
  if(mMembanks[topology].tag.find("HBM") != std::string::npos)
  {
	  return 2;
  }
  return 1;
}
  size_t HwEmShim::xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek, uint32_t topology)
  {
    if(!sock)
    {
      if(!mMemModel)
        mMemModel = new mem_model(deviceName);
      mMemModel->writeDevMem(dest,src,size);
      return size;
    }
    src = (unsigned char*)src + seek;
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
        << src << ", " << size << ", " << seek << std::endl;
    }
    std::string dMsg ="INFO: [HW-EM 02-0] Copying buffer from host to device started : size = " + std::to_string(size);
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
    dMsg ="INFO: [HW-EM 02-1] Copying buffer from host to device ended";
    logMessage(dMsg,1);

    PRINTENDFUNC;
    printMem(mGlobalInMemStream, 16 , dest , (void*)src, size );

    return size;
  }

  size_t HwEmShim::xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip, uint32_t topology)
  {
    dest = ((unsigned char*)dest) + skip;
    if(!sock)
    {
      if(!mMemModel)
        mMemModel = new mem_model(deviceName);
      mMemModel->readDevMem(src,dest,size);
      return size;
    }
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
        << src << ", " << size << ", " << skip << std::endl;
    }

    std::string dMsg ="INFO: [HW-EM 05-0] Copying buffer from device to host started. size := " + std::to_string(size);
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
      uint32_t space = getAddressSpace(topology);
      xclCopyBufferDevice2Host_RPC_CALL(xclCopyBufferDevice2Host,handle,c_dest,c_src,c_size,skip,space);
#endif

      processed_bytes += c_size;
    }
    dMsg ="INFO: [HW-EM 05-1] Copying buffer from device to host ended";
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
      bool p2pBuffer = false;
      std::string sFileName("");
      xclAllocDeviceBuffer_RPC_CALL(xclAllocDeviceBuffer,finalValidAddress,origSize,p2pBuffer);

      PRINTENDFUNC;
      if(!ack)
        return 0;
    }
    return finalValidAddress;
  }

  uint64_t HwEmShim::xclAllocDeviceBuffer2(size_t& size, xclMemoryDomains domain, unsigned flags, bool p2pBuffer, std::string &sFileName)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << size <<", "<<domain<<", "<< flags <<std::endl;
    }

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
    if(result == xclemulation::MemoryManager::mNull)
      return result;
    uint64_t finalValidAddress = result+(paddingFactor*size);
    uint64_t finalSize = size+(2*paddingFactor*size);
    mAddrMap[finalValidAddress] = finalSize;
    bool ack = false;
    if(sock)
    {
      xclAllocDeviceBuffer_RPC_CALL(xclAllocDeviceBuffer,finalValidAddress,origSize,p2pBuffer);

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
      if (buf < i->start() + i->size()) {
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
      char* pPath = GetCurrentDir(path,size);

      if(pPath)
      {
        // Copy waveform database
        std::string extension = "wdb";
        if (boost::filesystem::exists(binaryDirectory+"/msim"))
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
        std::string logFilePath= binaryDirectory + "/profile_kernels.csv";
        std::string destPath3 = "'" + std::string(path) + "/profile_kernels.csv'";
        systemUtil::makeSystemCall(logFilePath, systemUtil::systemOperation::APPEND, destPath3);
        xclemulation::copyLogsFromOneFileToAnother(logFilePath, mDebugLogStream);

        // Append to detailed kernel trace "timeline" file
        std::string traceFilePath = binaryDirectory + "/timeline_kernels.csv";
        std::string destPath4 = "'" + std::string(path) + "/timeline_kernels.csv'";
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
        
        // Copy Simulation Log file
        std::string sdxEmulatorLogFilePath= binaryDirectory + "/" + "sdx_emulator.log";
        std::string destPath7 = "'" + std::string(path) + "/" + fileName + "_sdx_emulator.log'";
        systemUtil::makeSystemCall(sdxEmulatorLogFilePath, systemUtil::systemOperation::COPY, destPath7);
        
        // Copy xsc_report Log file
        std::string xscReportLogFilePath= binaryDirectory + "/" + "xsc_report.log";
        std::string destPath8 = "'" + std::string(path) + "/" + fileName + "_xsc_report.log'";
        systemUtil::makeSystemCall(xscReportLogFilePath, systemUtil::systemOperation::COPY, destPath8);

      }
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
   
    for (auto& it: mFdToFileNameMap)
    {
      int fd=it.first;
      int sSize = std::get<1>(it.second);
      void* addr = std::get<2>(it.second);
      munmap(addr,sSize);
      close(fd);
    }
    mFdToFileNameMap.clear();

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
      std::string waitingMsg ="INFO: [HW-EM 06-0] Waiting for the simulator process to exit";
      logMessage(waitingMsg);
    }

    bool simDontRun = xclemulation::config::getInstance()->isDontRun();
    if(!simDontRun)
      while (-1 == waitpid(0, &status, 0));

    if(( lWaveform == xclemulation::LAUNCHWAVEFORM::GUI || lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH) && xclemulation::config::getInstance()->isInfoSuppressed() == false)
    {
      std::string waitingMsg ="INFO: [HW-EM 06-1] All the simulator processes exited successfully";
      logMessage(waitingMsg);
    }

    saveWaveDataBase();
    if( xclemulation::config::getInstance()->isKeepRunDirEnabled() == false)
      systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::REMOVE);
    google::protobuf::ShutdownProtobufLibrary();
    PRINTENDFUNC;
  }

  int HwEmShim::resetProgram(bool saveWdb)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }
    
     for (auto& it: mFdToFileNameMap)
    {
      int fd=it.first;
      int sSize = std::get<1>(it.second);
      void* addr = std::get<2>(it.second);
      munmap(addr,sSize);
      close(fd);
    }
    mFdToFileNameMap.clear();

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
      if (counter == XPAR_AIM0_HOST_SLOT)
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
    fetchAndPrintMessages();
    simulator_started = false;
    std::string socketName = sock->get_name();
    if(socketName.empty() == false)// device is active if socketName is non-empty
    {
#ifndef _WINDOWS
      xclClose_RPC_CALL(xclClose,this);
#endif
      closemMessengerThread();
      //clean up directories which are created inside the driver
      systemUtil::makeSystemCall(socketName, systemUtil::systemOperation::REMOVE);
    }

    if(saveWdb)
    {
      int status = 0;
      xclemulation::LAUNCHWAVEFORM lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();
      if(( lWaveform == xclemulation::LAUNCHWAVEFORM::GUI || lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH) && xclemulation::config::getInstance()->isInfoSuppressed() == false)
      {
        std::string waitingMsg ="INFO: [HW-EM 06-0] Waiting for the simulator process to exit";
        logMessage(waitingMsg);
      }

      bool simDontRun = xclemulation::config::getInstance()->isDontRun();
      if(!simDontRun)
        while (-1 == waitpid(0, &status, 0));

      if(( lWaveform == xclemulation::LAUNCHWAVEFORM::GUI || lWaveform == xclemulation::LAUNCHWAVEFORM::BATCH) && xclemulation::config::getInstance()->isInfoSuppressed() == false)
      {
        std::string waitingMsg ="INFO: [HW-EM 06-1] All the simulator processes exited successfully";
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
    if (!handle)
      return 0;

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
    if(mDataSpace)
    {
      delete mDataSpace;
      mDataSpace = NULL;
    }
    closemMessengerThread();
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
      mDDRMemoryManager.push_back(new xclemulation::MemoryManager(bankSize, base , getpagesize()));
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
    uint32_t numCdma = 0;
    if(isCdmaEnabled())
    {
      for(unsigned int i =0  ; i < 4; i++)
      {
        if ( getCdmaBaseAddress(i) != 0)
        {
          numCdma++;
        }
      }
    }
    dest->mNumCDMA = numCdma;
    for(unsigned int i = 0; i < 4 ;i++)
      dest->mOCLFrequency[i]       =    src->mOCLFrequency[i];

  }

  HwEmShim::HwEmShim( unsigned int deviceIndex, xclDeviceInfo2 &info, std::list<xclemulation::DDRBank>& DDRBankList, bool _unified, bool _xpr, FeatureRomHeader &fRomHeader)
    :mRAMSize(info.mDDRSize)
    ,mCoalesceThreshold(4)
    ,mDSAMajorVersion(DSA_MAJOR_VERSION)
    ,mDSAMinorVersion(DSA_MINOR_VERSION)
    ,mDeviceIndex(deviceIndex)
  {
    simulator_started = false;
    tracecount_calls = 0;
    mReqCounter = 0;

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
  
    std::memset(&mFeatureRom, 0, sizeof(FeatureRomHeader));
    std::memcpy(&mFeatureRom, &fRomHeader, sizeof(FeatureRomHeader));
    
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
      char* pPath = GetCurrentDir(path,size);
      if(pPath)
      {
        std::string sdxProfileKernelFile = std::string(path) + "/profile_kernels.csv";
        systemUtil::makeSystemCall(sdxProfileKernelFile, systemUtil::systemOperation::REMOVE);
        std::string sdxTraceKernelFile = std::string(path) + "/timeline_kernels.csv";
        systemUtil::makeSystemCall(sdxTraceKernelFile, systemUtil::systemOperation::REMOVE);
      }
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
    mStreamProfilingNumberSlots = 0;
    mPerfMonFifoCtrlBaseAddress = 0;
    mPerfMonFifoReadBaseAddress = 0;
    mTraceFunnelAddress = 0;
    mDataSpace = new xclemulation::MemoryManager(0x10000000, 0, getpagesize());
    mCuBaseAddress = 0x0;
    mMessengerThreadStarted = false;
    mIsTraceHubAvailable = false;
  }

  bool HwEmShim::isMBSchedulerEnabled()
  {
    bool mbSchEnabled = mFeatureRom.FeatureBitMap & FeatureBitMask::MB_SCHEDULER;
    bool QDMAPlatform = (getDsaVersion() == 60)? true: false;
    return mbSchEnabled && !QDMAPlatform;
  }

  bool HwEmShim::isCdmaEnabled()
  {
    return mFeatureRom.FeatureBitMap & FeatureBitMask::CDMA;
  }

  uint64_t HwEmShim::getCdmaBaseAddress(unsigned int index)
  {
    return mFeatureRom.CDMABaseAddress[index];
  }

  //following code is copied from core/pcie/driver/linux/xocl/subdev/feature_rom.c
  unsigned int HwEmShim::getDsaVersion()
  {
    std::string vbnv  = mDeviceInfo.mName;
    if(vbnv.empty())
      return 52;
    if (vbnv.find("5_0") != std::string::npos)
      return 50;
    else if (vbnv.find("qdma") != std::string::npos)
      return 60;
    else if ( (vbnv.find("5_1") != std::string::npos)
        || (vbnv.find("u200_xdma_201820_1") != std::string::npos))
      return 51;
    else if ((vbnv.find("5_2") != std::string::npos)
        ||   (vbnv.find("u200_xdma_201820_2") != std::string::npos )
        ||   (vbnv.find("u250_xdma_201820_1") != std::string::npos )
        ||   (vbnv.find("201830") != std::string::npos))
      return 52;
    else if (vbnv.find("5_3") != std::string::npos)
      return 53;
    else if (vbnv.find("6_0") != std::string::npos)
      return 60;
  
    return 52;
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
    if(xclemulation::config::getInstance()->isSystemDPAEnabled() == true) {
      return;
    }
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

    bool ack = true;
    std::string displayMsgs;
    std::string logMsgs;
    std::string stopMsgs;
    xclGetDebugMessages_RPC_CALL(xclGetDebugMessages,ack,force,displayMsgs,logMsgs,stopMsgs);
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
    char* pPath = GetCurrentDir(path,size);
    if(pPath)
    {
      std::string sdxProfileKernelFile = std::string(path) + "/profile_kernels.csv";
      systemUtil::makeSystemCall(sdxProfileKernelFile, systemUtil::systemOperation::REMOVE);
      std::string sdxTraceKernelFile = std::string(path) + "/timeline_kernels.csv";
      systemUtil::makeSystemCall(sdxTraceKernelFile, systemUtil::systemOperation::REMOVE);
    }
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

static bool check_bo_user_flags(HwEmShim* dev, unsigned flags)
{
	const unsigned ddr_count = dev->xocl_ddr_channel_count();
	unsigned ddr;

	if(ddr_count == 0)
		return false;

	if (flags == 0xffffffff)
		return true;

  ddr = xclemulation::xocl_bo_ddr_idx(flags,false);
  if (ddr > ddr_count)
		return false;

	return true;
}

xclemulation::drm_xocl_bo* HwEmShim::xclGetBoByHandle(unsigned int boHandle)
{
  auto it = mXoclObjMap.find(boHandle);
  if(it == mXoclObjMap.end())
    return nullptr;

  xclemulation::drm_xocl_bo* bo = (*it).second;
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
  PRINTENDFUNC;
  return 0;
}
/*****************************************************************************************/

/******************************** xclAllocBO *********************************************/
uint64_t HwEmShim::xoclCreateBo(xclemulation::xocl_create_bo* info)
{
	size_t size = info->size;
  unsigned ddr = xclemulation::xocl_bo_ddr_idx(info->flags,false);

  if (!size)
  {
    return -1;
  }

  /* Either none or only one DDR should be specified */
  if (!check_bo_user_flags(this, info->flags))
  {
    return -1;
  }

  struct xclemulation::drm_xocl_bo *xobj = new xclemulation::drm_xocl_bo;
  xobj->flags=info->flags;
  /* check whether buffer is p2p or not*/
  bool p2pBuffer = xocl_bo_p2p(xobj); 
  std::string sFileName("");
  
  if(xobj->flags & XCL_BO_FLAGS_EXECBUF)
  {
    uint64_t result = mDataSpace->alloc(size,1);
    xobj->base = result;
  }
  else
  {
    xobj->base = xclAllocDeviceBuffer2(size,XCL_MEM_DEVICE_RAM,ddr,p2pBuffer,sFileName);
  }
  xobj->filename = sFileName;
  xobj->size = size;
  xobj->userptr = NULL;
  xobj->buf = NULL;
  xobj->topology=ddr;
  xobj->fd = -1;
  if(xobj->base == xclemulation::MemoryManager::mNull)
  {
    return xclemulation::MemoryManager::mNull;
  }

  info->handle = mBufferCount;
  mXoclObjMap[mBufferCount++] = xobj;
  return 0;
}

unsigned int HwEmShim::xclAllocBO(size_t size, int unused, unsigned flags)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  if (mLogStream.is_open())
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << size << std::dec << " , "<< unused <<" , "<< flags << std::endl;
  }
  xclemulation::xocl_create_bo info = {size, mNullBO, flags};
  uint64_t result = xoclCreateBo(&info);
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
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << userptr <<", " << std::hex << size << std::dec <<" , "<< flags << std::endl;
  }
  xclemulation::xocl_create_bo info = {size, mNullBO, flags};
  uint64_t result = xoclCreateBo(&info);
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(info.handle);
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
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if(!bo)
    return -1;

  std::string sFileName = bo->filename;
  if(sFileName.empty())
  {
    std::cout<<"Exported Buffer is not P2P "<<std::endl;
    PRINTENDFUNC;
    return -1;
  }

  uint64_t size = bo->size;
  int fd = open(sFileName.c_str(), (O_CREAT | O_RDWR), 0666);
  if (fd == -1) 
  {
    printf("Error opening exported BO file.\n");
    PRINTENDFUNC;
    return -1;
  };

  char* data = (char*) mmap(0, bo->size , PROT_READ |PROT_WRITE |PROT_EXEC ,  MAP_SHARED, fd, 0);
  if(!data)
  {
    PRINTENDFUNC;
    return -1;
  }

  int rf = ftruncate(fd, bo->size);
  if(rf == -1 )
  {
    close(fd);
    munmap(data,size);
    return -1;
  }
  mFdToFileNameMap [fd] = std::make_tuple(sFileName,size,(void*)data);
  PRINTENDFUNC;
  return fd;
}
/***************************************************************************************/

/******************************** xclImportBO *******************************************/
unsigned int HwEmShim::xclImportBO(int boGlobalHandle, unsigned flags)
{
  //TODO
  if (mLogStream.is_open())
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << boGlobalHandle << std::endl;
  }
  auto itr = mFdToFileNameMap.find(boGlobalHandle);
  if(itr != mFdToFileNameMap.end())
  {
    const std::string& fileName = std::get<0>((*itr).second);
    int size = std::get<1>((*itr).second);

    unsigned int importedBo = xclAllocBO(size, 0, flags);
    xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(importedBo);
    if(!bo)
    {
      std::cout<<"ERROR HERE in importBO "<<std::endl;
      return -1;
    }
    mImportedBOs.insert(importedBo);
    bo->fd = boGlobalHandle;
    bool ack;
    xclImportBO_RPC_CALL(xclImportBO,fileName,bo->base,size);
    PRINTENDFUNC;
    if(!ack)
      return -1;
    return importedBo;
  }
  PRINTENDFUNC;
  return -1;

}
/***************************************************************************************/

/******************************** xclCopyBO *******************************************/
int HwEmShim::xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
   std::lock_guard<std::mutex> lk(mApiMtx);
  //TODO
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << std::hex << dst_boHandle 
      <<" , "<< src_boHandle << " , "<< size <<"," << dst_offset << "," <<src_offset<< std::endl;
  }
  xclemulation::drm_xocl_bo* sBO = xclGetBoByHandle(src_boHandle);
  if(!sBO)
  {
    PRINTENDFUNC;
    return -1;
  }

  xclemulation::drm_xocl_bo* dBO = xclGetBoByHandle(dst_boHandle);
  if(!dBO)
  {
    PRINTENDFUNC;
    return -1;
  }
  if(dBO->fd < 0)
  {
    std::cout<<"bo is not exported for copying"<<std::endl;
    return -1;
  }

  int ack = false;
  auto fItr = mFdToFileNameMap.find(dBO->fd);
  if(fItr != mFdToFileNameMap.end())
  {
    const std::string& sFileName = std::get<0>((*fItr).second);
    xclCopyBO_RPC_CALL(xclCopyBO,sBO->base,sFileName,size,src_offset,dst_offset);
  }
  if(!ack)
    return -1;
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
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if (!bo) {
    PRINTENDFUNC;
    return nullptr;
  }

  std::string sFileName = bo->filename;
  if(!sFileName.empty() )
  {
    int fd = open(sFileName.c_str(), (O_CREAT | O_RDWR), 0666);
    if (fd == -1) 
    {
      printf("Error opening exported BO file.\n");
      return nullptr;
    };

    char* data = (char*) mmap(0, bo->size , PROT_READ |PROT_WRITE |PROT_EXEC ,  MAP_SHARED, fd, 0);
    if(!data)
      return nullptr;

    int rf = ftruncate(fd, bo->size);
    if(rf == -1)
    {
      close(fd);
      munmap(data,bo->size);
      return nullptr;
    }
    mFdToFileNameMap [fd] = std::make_tuple(sFileName,bo->size,(void*)data);
    bo->buf = data;
    PRINTENDFUNC;
    return data;
  }

  void *pBuf=nullptr;
  if (posix_memalign(&pBuf, sizeof(double)*16, bo->size))
  {
    if (mLogStream.is_open()) mLogStream << "posix_memalign failed" << std::endl;
    pBuf=nullptr;
    return pBuf;
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
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if(!bo)
  {
    PRINTENDFUNC;
    return -1;
  }

  int returnVal = 0;
  if(dir == XCL_BO_SYNC_BO_TO_DEVICE)
  {
    void* buffer =  bo->userptr ? bo->userptr : bo->buf;
    if (xclCopyBufferHost2Device(bo->base, buffer, size, offset, bo->topology) != size)
    {
      returnVal = EIO;
    }
  }
  else
  {
    void* buffer =  bo->userptr ? bo->userptr : bo->buf;
    if (xclCopyBufferDevice2Host(buffer, bo->base, size,offset, bo->topology) != size)
    {
      returnVal = EIO;
    }
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
size_t HwEmShim::xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek)
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
  size_t returnVal = 0;
  if (xclCopyBufferHost2Device(bo->base, src, size, seek, bo->topology) != size)
  {
    returnVal = EIO;
  }
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
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if(!bo)
  {
    PRINTENDFUNC;
    return -1;
  }
  size_t returnVal = 0;
  if (xclCopyBufferDevice2Host(dst, bo->base, size, skip, bo->topology) != size)
  {
    returnVal = EIO;
  }
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
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(cmdBO);
  if(!mMBSch || !bo)
  {
    PRINTENDFUNC;
    return -1;
  }
  int ret = mMBSch->add_exec_buffer(mCore, bo);
  PRINTENDFUNC;
  return ret;
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

ssize_t HwEmShim::xclUnmgdPwrite(unsigned flags, const void *buf, size_t count, uint64_t offset)
{
  if (flags)
    return -EINVAL;
  return xclCopyBufferHost2Device(offset, buf, count, 0 ,0);
}

ssize_t HwEmShim::xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset)
{
  if (flags)
    return -EINVAL;
  return xclCopyBufferDevice2Host(buf, offset, count, 0 , 0);
}


int HwEmShim::xclGetDebugIPlayoutPath(char* layoutPath, size_t size)
{
  // get path of the debug_ip_layout (in binary format) created in the HW Emu run directory
  if(mRunDeviceBinDir.empty())
    return -1;

  std::string debugIPlayoutPath = mRunDeviceBinDir + "/debug_ip_layout";
  if(debugIPlayoutPath.size() >= size)
    return -1;
  
  strncpy(layoutPath, debugIPlayoutPath.c_str(), size);
  return 0;
}

int HwEmShim::xclGetTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  uint32_t bytesPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 8);

  traceBufSz = MAX_TRACE_NUMBER_SAMPLES * bytesPerSample;   /* Buffer size in bytes */
  traceSamples = nSamples;

  return 0;
}

int HwEmShim::xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
    // Create trace buffer on host (requires alignment)
    const int traceBufWordSz = traceBufSz / 4;  // traceBufSz is in number of bytes

    uint32_t size = 0;

    wordsPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 32);
    uint32_t numWords = numSamples * wordsPerSample;

//    alignas is defined in c++11
#if GCC_VERSION >= 40800
    /* Alignment is limited to 16 by PPC64LE : so , should it be
    alignas(16) uint32_t hostbuf[traceBufSzInWords];
    */
    alignas(AXI_FIFO_RDFD_AXI_FULL) uint32_t hostbuf[traceBufWordSz];
#else
    xrt_core::AlignedAllocator<uint32_t> alignedBuffer(AXI_FIFO_RDFD_AXI_FULL, traceBufWordSz);
    uint32_t* hostbuf = alignedBuffer.getBuffer();
#endif

    // Now read trace data
    memset((void *)hostbuf, 0, traceBufSz);

    // Iterate over chunks
    // NOTE: AXI limits this to 4K bytes per transfer
    uint32_t chunkSizeWords = 256 * wordsPerSample;
    if (chunkSizeWords > 1024) chunkSizeWords = 1024;
    uint32_t chunkSizeBytes = 4 * chunkSizeWords;
    uint32_t words=0;

    // Read trace a chunk of bytes at a time
    if (numWords > chunkSizeWords) {
      for (; words < (numWords-chunkSizeWords); words += chunkSizeWords) {
          if(mLogStream.is_open())
            mLogStream << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
                          << std::hex << ipBaseAddress /*fifoReadAddress[0] or AXI_FIFO_RDFD*/ << " and writing it to 0x"
                          << (void *)(hostbuf + words) << std::dec << std::endl;

        xclUnmgdPread(0 /*flags*/, (void *)(hostbuf + words) /*buf*/, chunkSizeBytes /*count*/, ipBaseAddress /*offset : or AXI_FIFO_RDFD*/);

        size += chunkSizeBytes;
      }
    }

    // Read remainder of trace not divisible by chunk size
    if (words < numWords) {
      chunkSizeBytes = 4 * (numWords - words);

      if(mLogStream.is_open()) {
        mLogStream << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
                      << std::hex << ipBaseAddress /*fifoReadAddress[0]*/ << " and writing it to 0x"
                      << (void *)(hostbuf + words) << std::dec << std::endl;
      }

      xclUnmgdPread(0 /*flags*/, (void *)(hostbuf + words) /*buf*/, chunkSizeBytes /*count*/, ipBaseAddress /*offset : or AXI_FIFO_RDFD*/);

      size += chunkSizeBytes;
    }

    if(mLogStream.is_open())
        mLogStream << __func__ << ": done reading " << size << " bytes " << std::endl;

    memcpy((char*)traceBuf, (char*)hostbuf, traceBufSz);

    return size;
}



/********************************************** QDMA APIs IMPLEMENTATION START **********************************************/

/*
 * xclCreateWriteQueue()
 */
int HwEmShim::xclCreateWriteQueue(xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  
  if (mLogStream.is_open()) 
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;

  uint64_t q_handle = 0;
  xclCreateQueue_RPC_CALL(xclCreateQueue,q_ctx,true);
  if(q_handle <= 0)
  {
    if (mLogStream.is_open()) 
      mLogStream << " unable to create write queue "<<std::endl;
    PRINTENDFUNC;
    return -1;
  }
  *q_hdl = q_handle;
  PRINTENDFUNC;
  return 0;
}

/*
 * xclCreateReadQueue()
 */
int HwEmShim::xclCreateReadQueue(xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
  }
  uint64_t q_handle = 0;
  xclCreateQueue_RPC_CALL(xclCreateQueue,q_ctx,false);
  if(q_handle <= 0)
  {
    if (mLogStream.is_open()) 
      mLogStream << " unable to create read queue "<<std::endl;
    PRINTENDFUNC;
    return -1;
  }
  *q_hdl = q_handle;
  PRINTENDFUNC;
  return 0;
}

/*
 * xclDestroyQueue()
 */
int HwEmShim::xclDestroyQueue(uint64_t q_hdl)
{
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
  }
  uint64_t q_handle = q_hdl;
  bool success = false;
  xclDestroyQueue_RPC_CALL(xclDestroyQueue, q_handle);
  if(!success)
  {
    if (mLogStream.is_open()) 
      mLogStream <<" unable to destroy the queue"<<std::endl;
    PRINTENDFUNC;
    return -1;
  }

  PRINTENDFUNC;
  return 0;
}

/*
 * xclWriteQueue()
 */
ssize_t HwEmShim::xclWriteQueue(uint64_t q_hdl, xclQueueRequest *wr)
{
  
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
  }

  bool eot = false;
  if(wr->flag & XCL_QUEUE_REQ_EOT)
    eot = true;
  
  bool nonBlocking = false;
  if (wr->flag & XCL_QUEUE_REQ_NONBLOCKING) 
  {
    std::map<uint64_t,uint64_t> vaLenMap;
    for (unsigned i = 0; i < wr->buf_num; i++) 
    {
      //vaLenMap[wr->bufs[i].va] = wr->bufs[i].len;
      vaLenMap[wr->bufs[i].va] = 0;//for write we should not read the data back
    }
    mReqList.push_back(std::make_tuple(mReqCounter, wr->priv_data, vaLenMap));
    nonBlocking = true;
  }
  uint64_t fullSize = 0;
  for (unsigned i = 0; i < wr->buf_num; i++) 
  {
    xclWriteQueue_RPC_CALL(xclWriteQueue,q_hdl, wr->bufs[i].va, wr->bufs[i].len);
    fullSize += written_size;
  }
  PRINTENDFUNC;
  mReqCounter++;
  return fullSize;
}

/*
 * xclReadQueue()
 */
ssize_t HwEmShim::xclReadQueue(uint64_t q_hdl, xclQueueRequest *rd)
{
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
  }
  
  bool eot = false;
  if(rd->flag & XCL_QUEUE_REQ_EOT)
    eot = true;

  bool nonBlocking = false;
  if (rd->flag & XCL_QUEUE_REQ_NONBLOCKING) 
  {
    nonBlocking = true;
    std::map<uint64_t,uint64_t> vaLenMap;
    for (unsigned i = 0; i < rd->buf_num; i++) 
    {
      vaLenMap[rd->bufs[i].va] = rd->bufs[i].len;
    }
    mReqList.push_back(std::make_tuple(mReqCounter,rd->priv_data, vaLenMap));
  }

  void *dest;

  uint64_t fullSize = 0;
  for (unsigned i = 0; i < rd->buf_num; i++) 
  {
    dest = (void *)rd->bufs[i].va;
    uint64_t read_size = 0;
    do
    {
      xclReadQueue_RPC_CALL(xclReadQueue,q_hdl, dest , rd->bufs[i].len);
    } while (read_size == 0 && !nonBlocking);
    fullSize += read_size;
  }
  mReqCounter++;
  PRINTENDFUNC;
  return fullSize;

}
/*
 * xclPollCompletion
 */
int HwEmShim::xclPollCompletion(int min_compl, int max_compl, xclReqCompletion *comps, int* actual, int timeout)
{
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << " , "<< max_compl <<", "<<min_compl<<" ," << *actual <<" ," << timeout << std::endl;
  }
//  struct timespec time, *ptime = NULL;
//
//  if (timeout > 0) 
//  {
//    memset(&time, 0, sizeof(time));
//    time.tv_sec = timeout / 1000;
//    time.tv_nsec = (timeout % 1000) * 1000000;
//    ptime = &time;
//  }

  *actual = 0;
  while(*actual < min_compl)
  {
    std::list<std::tuple<uint64_t ,void*, std::map<uint64_t,uint64_t> > >::iterator it = mReqList.begin();
    while ( it != mReqList.end() )
    {
      unsigned numBytesProcessed = 0;
      uint64_t reqCounter = std::get<0>(*it);
      void* priv_data = std::get<1>(*it);
      std::map<uint64_t,uint64_t>vaLenMap = std::get<2>(*it);
      xclPollCompletion_RPC_CALL(xclPollCompletion,reqCounter,vaLenMap);
      if(numBytesProcessed > 0)
      {
        comps[*actual].priv_data = priv_data;
        comps[*actual].nbytes = numBytesProcessed;
        (*actual)++;
        mReqList.erase(it++);
      }
      else
      {
        it++;
      }
    }
  }
  PRINTENDFUNC;
  return (*actual);
}

/*
 * xclAllocQDMABuf()
 */
void * HwEmShim::xclAllocQDMABuf(size_t size, uint64_t *buf_hdl)
{
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
  }
  void *pBuf=nullptr;
  if (posix_memalign(&pBuf, sizeof(double)*16, size))
  {
    if (mLogStream.is_open()) mLogStream << "posix_memalign failed" << std::endl;
    pBuf=nullptr;
    return pBuf;
  }
  memset(pBuf, 0, size);
  return pBuf;
}

/*
 * xclFreeQDMABuf()
 */
int HwEmShim::xclFreeQDMABuf(uint64_t buf_hdl)
{
  
  if (mLogStream.is_open()) 
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
  }
  PRINTENDFUNC;
  return 0;//TODO
}

/*
 * xclLogMsg()
 */
int HwEmShim::xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char* tag, const char* format, va_list args1)
{
    int len = std::vsnprintf(nullptr, 0, format, args1);

    if (len < 0) 
    {
        //illegal arguments
        std::string err_str = "ERROR: Illegal arguments in log format string. ";
        err_str.append(std::string(format));
        xrt_core::message::send((xrt_core::message::severity_level)level, tag, err_str.c_str());
        return len;
    }
    len++; //To include null terminator

    std::vector<char> buf(len);
    len = std::vsnprintf(buf.data(), len, format, args1);

    if (len < 0) 
    {
        //error processing arguments
        std::string err_str = "ERROR: When processing arguments in log format string. ";
        err_str.append(std::string(format));
        xrt_core::message::send((xrt_core::message::severity_level)level, tag, err_str.c_str());
        return len;
    }
    xrt_core::message::send((xrt_core::message::severity_level)level, tag, buf.data());
    return 0;
}

void HwEmShim::closemMessengerThread() {
	if(mMessengerThreadStarted) {
		mMessengerThread.join();
		mMessengerThreadStarted = false;
	}
}
/********************************************** QDMA APIs IMPLEMENTATION END**********************************************/
/**********************************************HAL2 API's END HERE **********************************************/
}  // end namespace xclhwemhal2
