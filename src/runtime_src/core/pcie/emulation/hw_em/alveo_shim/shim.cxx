// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#include "shim.h"
#include "system_hwemu.h"
#include "plugin/xdp/device_offload.h"

#include "core/include/xclbin.h"

#include "core/common/AlignedAllocator.h"
#include "core/common/xclbin_parser.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/api/xclbin_int.h"

#include <unistd.h>

#include <boost/property_tree/xml_parser.hpp>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <mutex>
#include <set>
#include <vector>

#include "core/include/xdp/fifo.h"
#include "core/include/xdp/trace.h"

#define SEND_RESP2QDMA() \
    { \
        auto raw_response_header    = std::make_unique<char[]>(ri_len); \
        auto raw_response_payload   = std::make_unique<char[]>(r_len);\
        response_header->set_size(r_len);\
        response_header->SerializeToArray((void*)raw_response_header.get(),ri_len);\
        response_payload.SerializeToArray((void*)raw_response_payload.get(),r_len);\
        Q2h_sock->sk_write((void*)raw_response_header.get(),ri_len);\
        Q2h_sock->sk_write((void*)raw_response_payload.get(),r_len);\
    }




namespace xclhwemhal2 {
    //Thread for which pooling for transaction from SIM_QDMA

    /**
      * helper class for transactions from SIM_QDMA to XRT
      *
      */
    class Q2H_helper {
        private:
        std::unique_ptr<call_packet_info> header;
        std::unique_ptr<response_packet_info> response_header;
        size_t  i_len;
        size_t  ri_len;
        std::unique_ptr<unix_socket> Q2h_sock;
        xclhwemhal2::HwEmShim* inst;

        public:
        Q2H_helper(xclhwemhal2::HwEmShim* _inst);
        ~Q2H_helper();
        int  poolingon_Qdma();
        bool connect_sock();
    };

  namespace pt = boost::property_tree;
  namespace fs = boost::filesystem;

  std::map<unsigned int, HwEmShim*> devices;
  std::map<std::string, std::string> HwEmShim::mEnvironmentNameValueMap(xclemulation::getEnvironmentByReadingIni());
  std::map<int, std::tuple<std::string,int,void*, unsigned int> > HwEmShim::mFdToFileNameMap;
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
  const unsigned HwEmShim::REG_BUFF_SIZE = 0x4;
  const unsigned HwEmShim::M2M_KERNEL_ARGS_SIZE = 36;


  // Maintain a list of all currently open device handles.
  //
  // xclClose removes a handle from the list.  At static destruction
  // the list of still open handles is iterated and devices are
  // manually closed.  This ensures xclClose is called for all opened
  // devices even when upper level code (OpenCL in particular) doesn't
  // close all acquired resources.
  //
  // The static handles must be descructed before other static data
  // members, so it is important device_handles::handles is defined
  // after above initializations.  Order of static destruciton is in
  // reverse order of construction.
  //
  // Ideally all statics should be managed in the unnamed namespace of
  // this compilation unit and not be data members of the shim class.
  namespace device_handles {
    static std::mutex mutex;
    static std::set<xclDeviceHandle> handles;

    inline void
    add(xclDeviceHandle hdl)
    {
      std::lock_guard<std::mutex> lk(mutex);
      handles.insert(hdl);
    }

    inline void
    remove(xclDeviceHandle hdl)
    {
      std::lock_guard<std::mutex> lk(mutex);
      handles.erase(hdl);
    }

    struct X
    {
      ~X()
      {
        auto end = handles.end();
        for (auto itr = handles.begin(); itr != end;) {
          xclClose(*itr);  // removes handle from handles
          itr = handles.begin();
        }
      }
    };

    static X x;
  }

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
      void *result = realloc(buf, new_size);
      // If realloc was unsuccessful, then give up and deallocate.
      if (!result)
      {
        free(buf);
        buf = nullptr;
        return 0;
      }
      buf = result;
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

      if (xclemulation::config::getInstance()->isKeepRunDirEnabled() == false) {
        systemUtil::makeSystemCall(handle->deviceDirectory, systemUtil::systemOperation::REMOVE, "", std::to_string(__LINE__));
      }
    }

  }

  std::string HwEmShim::loadFileContentsToString(const std::string& path)
  {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  }

  int HwEmShim::parseLog()
  {
    auto lstatus = 0;
    std::vector<std::string> myvector = {"SIM-IPC's external process can be connected to instance",
                                         "SystemC TLM functional mode",
                                         "HLS_PRINT"};

    std::ifstream ifs(getSimPath() + "/simulate.log");

    if (ifs.is_open()) {
      std::string line;
      while (std::getline(ifs, line)) {
        // If xsim is terminated by logging in simulate.log then call xclClose to properly clean all instances, threads.
        if ( (std::string::npos != line.find("Exiting xsim")) || 
             (std::string::npos != line.find("ERROR")) 
           ) {
                  std::cout << "SIMULATION EXITED" << std::endl;
                  lstatus = -1;
                  parsedMsgs.push_back(line);
                  this->xclClose(true);                                               // Let's have a proper clean if xsim is NOT running
        }
        for (auto& matchString : myvector) {
          std::string::size_type index = line.find(matchString);
          if (index == std::string::npos) 
            continue;
          
          if(std::find(parsedMsgs.begin(), parsedMsgs.end(), line) != parsedMsgs.end())
            continue;
          
          logMessage(line);
          parsedMsgs.push_back(line);
        }
      }
    }
    return lstatus;
  }

  void HwEmShim::parseString(const std::string& simPath , const std::string& searchString)
  {
    std::ifstream ifs(simPath + "/simulate.log");
    std::string lineHandle;
    while(getline(ifs, lineHandle)) {
      if(lineHandle.find(searchString, 0) != std::string::npos)
        logMessage(lineHandle);
    }
  }

  void HwEmShim::dumpDeadlockMessages()
  {
    std::string simPath = getSimPath();
    std::string content = loadFileContentsToString(simPath + "/kernel_deadlock_diagnosis.rpt");

    if (content.find("start to dump deadlock path") != std::string::npos)
    {
      if (std::find(parsedMsgs.begin(), parsedMsgs.end(), content) == parsedMsgs.end())
      {
        logMessage(content);
        parsedMsgs.push_back(content);
      }
    }

    if (!xrt_core::config::get_pl_deadlock_detection())
      return;

    char path[FILENAME_MAX];
    size_t size = MAXPATHLEN;
    char *pPath = GetCurrentDir(path, size);

    if (pPath)
    {
      std::string deadlockReportFile = simPath + "/kernel_deadlock_diagnosis.rpt";
      if (boost::filesystem::exists(deadlockReportFile))
      {
        std::string destPath = std::string(path) + "/pl_deadlock_diagnosis.txt";
        systemUtil::makeSystemCall(deadlockReportFile, systemUtil::systemOperation::COPY, destPath, std::to_string(__LINE__));
      }
    }
  }

  void HwEmShim::parseSimulateLog ()
  {
    std::string simPath = getSimPath();
    std::string content = loadFileContentsToString(simPath + "/simulate.log");
    if (content.find("// ERROR!!! DEADLOCK DETECTED ") != std::string::npos) {
      size_t first = content.find("// ERROR!!! DEADLOCK DETECTED");
      size_t last = content.find("detected!", first);
      // substr including the word detected!
      std::string deadlockMsg = content.substr(first , last + 9 - first);
      logMessage(deadlockMsg);
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

    std::unique_ptr<char[]> zipFile;
    std::unique_ptr<char[]> xmlFile;
    std::unique_ptr<char[]> debugFile;
    std::unique_ptr<char[]> memTopology;
    std::unique_ptr<char[]>  pdi;
    std::unique_ptr<char[]> emuData;

    if (std::memcmp(bitstreambin, "xclbin2", 7)) {
      PRINTENDFUNC;
      return -1;
    }
    //check xclbin version with vivado tool version
    xclemulation::checkXclibinVersionWithTool(header);

    auto top = reinterpret_cast<const axlf*>(header);
    if (auto sec = xclbin::get_axlf_section(top, EMBEDDED_METADATA)) {
      xmlFileSize = sec->m_sectionSize;
      xmlFile = std::make_unique<char[]>(xmlFileSize);
      memcpy(xmlFile.get(), bitstreambin + sec->m_sectionOffset, xmlFileSize);
    }
    if (auto sec = xclbin::get_axlf_section(top, BITSTREAM)) {
      zipFileSize = sec->m_sectionSize;
      zipFile = std::make_unique<char[]>(zipFileSize);
      memcpy(zipFile.get(), bitstreambin + sec->m_sectionOffset, zipFileSize);
    }
    if (auto sec = xclbin::get_axlf_section(top, DEBUG_IP_LAYOUT)) {
      debugFileSize = sec->m_sectionSize;
      debugFile = std::make_unique<char[]>(debugFileSize);
      memcpy(debugFile.get(), bitstreambin + sec->m_sectionOffset, debugFileSize);
    }
    if (auto sec = xrt_core::xclbin::get_axlf_section(top, ASK_GROUP_TOPOLOGY)) {
      memTopologySize = sec->m_sectionSize;
      memTopology = std::make_unique<char[]>(memTopologySize);
      memcpy(memTopology.get(), bitstreambin + sec->m_sectionOffset, memTopologySize);
    }
    if (auto sec = xclbin::get_axlf_section(top, PDI)) {
      pdiSize = sec->m_sectionSize;
      pdi = std::make_unique<char[]>(pdiSize);
      memcpy(pdi.get(), bitstreambin + sec->m_sectionOffset, pdiSize);
    }
    if (auto sec = xclbin::get_axlf_section(top, EMULATION_DATA)) {
      emuDataSize = sec->m_sectionSize;
      emuData = std::make_unique<char[]>(emuDataSize);
      memcpy(emuData.get(), bitstreambin + sec->m_sectionOffset, emuDataSize);
    }

    bitStreamArg loadBitStreamArgs;
    loadBitStreamArgs.m_zipFile = zipFile.get();
    loadBitStreamArgs.m_zipFileSize = zipFileSize;
    loadBitStreamArgs.m_xmlfile = xmlFile.get();
    loadBitStreamArgs.m_xmlFileSize = xmlFileSize;
    loadBitStreamArgs.m_debugFile = debugFile.get();
    loadBitStreamArgs.m_debugFileSize = debugFileSize;
    loadBitStreamArgs.m_memTopology = memTopology.get();
    loadBitStreamArgs.m_memTopologySize = memTopologySize;
    loadBitStreamArgs.m_pdi = pdi.get();
    loadBitStreamArgs.m_pdiSize = pdiSize;
    loadBitStreamArgs.m_emuData = emuData.get();
    loadBitStreamArgs.m_emuDataSize = emuDataSize;
    loadBitStreamArgs.m_top = top;

    int returnValue = xclLoadBitstreamWorker(loadBitStreamArgs);

    //mFirstBinary is a static member variable which becomes false once first binary gets loaded
    if(returnValue >=0 && mFirstBinary )
    {
      HwEmShim::mDebugLogStream.open(xclemulation::getEmDebugLogFile(),std::ofstream::out);
      if(xclemulation::config::getInstance()->isInfoSuppressed() == false)
      {
        std::string initMsg ="INFO: [HW-EMU 01] Hardware emulation runs simulation underneath. Using a large data set will result in long simulation times. It is recommended that a small dataset is used for faster execution. The flow uses approximate models for Global memories and interconnect and hence the performance data generated is approximate.";
        logMessage(initMsg);
      }
      mFirstBinary = false;
    }
    if(xclemulation::config::getInstance()->isNewMbscheduler()) {
        m_scheduler = new hwemu::xocl_scheduler(this);
    } else if (xclemulation::config::getInstance()->isXgqMode()) {
        m_xgq = new hwemu::xocl_xgq(this);
        if (m_xgq && pdi && pdiSize > 0) {
            returnValue = m_xgq->load_xclbin(pdi.get(), pdiSize);
        }
    } else {
        mCore = new exec_core;
        mMBSch = new MBScheduler(this);
        mMBSch->init_scheduler_thread();
    }

    PRINTENDFUNC;
    return returnValue;
  }

  int HwEmShim::xclLoadBitstreamWorker(bitStreamArg args)
  {
    bool is_enable_debug = xrt_core::config::get_is_enable_debug();
    std::string aie_sim_options = xrt_core::config::get_aie_sim_options();

    if (mLogStream.is_open()) {
         mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " <<  std::endl;
    }
    mCuIndx = 0;
    //TBD the file read may slowdown things...whenever xclLoadBitStream hal API implementation changes, we also need to make changes.

    boost::format fmt = boost::format("%1%/tempFile_%2%.zip") % deviceDirectory.c_str() % std::to_string(binaryCounter);
    std::string zip_fileName = fmt.str();

    if (mMemModel)
    {
      delete mMemModel;
      mMemModel = NULL;
    }

    if (sock)
    {
      resetProgram();
    }

    //CR-1116870 changes. Clearing "mOffsetInstanceStreamMap" for each kernel in case of multiple kernels with different xclbin's
    //required for sys_opt/kernel_swap example
    mOffsetInstanceStreamMap.clear();

    std::stringstream ss;
    ss << deviceDirectory << "/binary_" << binaryCounter;
    std::string binaryDirectory = ss.str();

    systemUtil::makeSystemCall(binaryDirectory, systemUtil::systemOperation::CREATE, "", std::to_string(__LINE__));
    systemUtil::makeSystemCall(binaryDirectory, systemUtil::systemOperation::PERMISSIONS, "777", std::to_string(__LINE__));

    mRunDeviceBinDir = binaryDirectory;

    std::ofstream os(zip_fileName);
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

      if (mLogStream.is_open())
         mLogStream << __func__ << " Creating the DDRMemoryManager Object with RTD section info" << std::endl;

      for (auto it : mMembanks)
      {
        //CR 966701: alignment to 4k (instead of mDeviceInfo.mDataAlignment)
        mDDRMemoryManager.push_back(new xclemulation::MemoryManager(it.size, it.base_addr, getpagesize(), it.tag));

        std::size_t found = it.tag.find("HOST");
        if (found != std::string::npos) {
          host_sptag_idx = it.index;
        }
      }

      for (auto it : mDDRMemoryManager)
      {
        std::string tag = it->tag();

        //continue if not MBG group
        if (tag.find("MBG") == std::string::npos) {
          continue;
        }

        // Connectivity provided with the bus direction for HBM[31:0], XCLBIN creates the large group of memory with all the HBM[31:0] size
        // like MBG. It indicates allocation of sequential memory is possible and not to limited size of one HBM. Hence creating the
        // HBM child memories (HBM subsets listed in RTD which falls under the range of MBG) for MBG memory type
        for (auto it2 : mDDRMemoryManager)
        {
          if (it2->size() != 0 && it2 != it &&
            it->start() <= it2->start() &&
            (it->start() + it->size()) >= (it2->start() + it2->size()))
          {
            //add HBM child memories to MBG large group[
            it->mChildMemories.push_back(it2);
          }
        }
      }
    }

    //CR-1116870 Changes Start

    auto projectName = xrt_core::xclbin::get_project_name(args.m_top);
    xrt::xclbin xclbin_object{args.m_top};
    auto fpgaDeviceName = xclbin_object.get_fpga_device_name();

    //New DRC check for Versal Platforms
    if (!fpgaDeviceName.empty() && fpgaDeviceName.find("versal:") != std::string::npos) {
      mVersalPlatform=true;
      if ((args.m_emuData == nullptr) && (args.m_emuDataSize <= 0)) {
        std::string dMsg = "ERROR: [HW-EMU 09] EMULATION_DATA section is missing in XCLBIN. This is a mandatory section required for Versal platforms. Please ensure the design is built with 'v++ -package' step, which inserts EMULATION_DATA into the XCLBIN.";
        logMessage(dMsg, 0);
        return -1;
      }
    }
    if (xclemulation::config::getInstance()->isSharedFmodel() && !mVersalPlatform) {
      setenv("SDX_USE_SHARED_MEMORY","true",true);
    }

    std::string instance_name = "";
    //CR-1122692:'auto' is treated as 32 bit int.But 'uint64_t' is needed
    uint64_t base_address = 0;
    for (const auto& kernel : xclbin_object.get_kernels())
    {
      // get properties of each kernel object
      auto props = xrt_core::xclbin_int::get_properties(kernel);
      //get CU's of each kernel object
      //iterate over CU's to get arguments
      for (const auto& cu : kernel.get_cus())
      {
        base_address = cu.get_base_address();
        //CR-1122692: Adding checks to validate base_address and adding 'mVersalPlatform' check
        if (base_address != (uint64_t)-1 && mVersalPlatform) {
          mCuBaseAddress = base_address & 0xFFFFFFFF00000000;
          //BAD Worharound for vck5000 need to remove once SIM_QDMA supports PCIE bar
          if(xclemulation::config::getInstance()->getCuBaseAddrForce()!=-1)
          {
            mCuBaseAddress = xclemulation::config::getInstance()->getCuBaseAddrForce();
          }
          else if(mVersalPlatform)
          {
            mCuBaseAddress = 0x20200000000;
          }
        }
        //fetch instance name
        instance_name = cu.get_name();
        if (xclemulation::config::getInstance()->isMemLogsEnabled())
        {
          //trim instance_name to get actual instance name
          auto position = instance_name.find(":");
          auto trimmed_instance_name = instance_name.substr(position+1);
          mOffsetInstanceStreamMap.emplace(std::make_pair(base_address,new std::ofstream(trimmed_instance_name + "_control.mem")));
        }

        if (props.address_range != 0 && !props.name.empty() && !instance_name.empty())
        {
          mCURangeMap[instance_name] = props.address_range;
        }
      }
    }
    std::string xclBinName = projectName;

    //CR-1116870 Changes End


    bool simDontRun = xclemulation::config::getInstance()->isDontRun();
    std::string launcherArgs = xclemulation::config::getInstance()->getLauncherArgs();
    std::string wdbFileName("");
    std::string kernelProfileFileName("profile_kernels.csv");
    std::string kernelTraceFileName("timeline_kernels.csv");
    // The following is evil--hardcoding. This name may change.
    // Is there a way we can determine the name from the directories or otherwise?
    std::string bdName("dr"); // Used to be opencldesign. This is new default.

    unsetenv("VITIS_WAVEFORM_WDB_FILENAME");
    unsetenv("VITIS_KERNEL_PROFILE_FILENAME");
    unsetenv("VITIS_KERNEL_TRACE_FILENAME");

    if (args.m_emuData) {
      extractEmuData(binaryDirectory, binaryCounter, args);
      std::string emuSettingsFilePath = binaryDirectory + "/emulation_data/emu_meta_data.json";
      readEmuSettingsJsonFile(emuSettingsFilePath);
    }

    //xrt.ini setting (dont_run=true) is getting the high priority
    if (simDontRun) {
      mSimDontRun = simDontRun;
    }

    std::string userSpecifiedSimPath = xclemulation::config::getInstance()->getSimDir();
    char *login_user = getenv("USER");
    if (!login_user)
    {
      std::string dMsg = "ERROR: [HW-EMU 26] $USER variable is not SET. Please make sure the USER env variable is set properly.";
      logMessage(dMsg, 0);
      exit(EXIT_FAILURE);
    }
    char *vitisInstallEnvvar = getenv("XILINX_VITIS");
    if (!vitisInstallEnvvar) {
      std::string dMsg = "ERROR: [HW-EMU 27] $XILINX_VITIS variable is not SET. Please make sure the XILINX_VITIS env variable is SOURCED properly.";
      logMessage(dMsg, 0);
      exit(EXIT_FAILURE);
    }
    if (!mSimDontRun)
    {
      wdbFileName = std::string(mDeviceInfo.mName) + "-" + std::to_string(mDeviceIndex) + "-" + xclBinName;
      xclemulation::debug_mode lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();

      if (lWaveform == xclemulation::debug_mode::gdb) {
        std::string dMsg = "ERROR: [HW-EMU 21] debug_mode option 'gdb' is no more valid. Valid options for debug_mode are 'gui', 'batch' and 'off'. Please make sure you build the application with 'wdb' mode";
        logMessage(dMsg, 0);
        return -1;
      }

      if (userSpecifiedSimPath.empty())
      {
        if (mLogStream.is_open())
          mLogStream << __func__ << " UNZIP of sim bin started" << std::endl;

        systemUtil::makeSystemCall(zip_fileName, systemUtil::systemOperation::UNZIP, binaryDirectory, std::to_string(__LINE__));

        if (mLogStream.is_open())
          mLogStream << __func__ << " UNZIP of sim bin complete" << std::endl;

        systemUtil::makeSystemCall(binaryDirectory, systemUtil::systemOperation::PERMISSIONS, "777", std::to_string(__LINE__));

        if (mLogStream.is_open())
          mLogStream << __func__ << " Permissions operation is complete" << std::endl;

        simulatorType = getSimulatorType(binaryDirectory);
        std::transform(simulatorType.begin(), simulatorType.end(), simulatorType.begin(), [](unsigned char c){return std::tolower(c);});
      }

      if (lWaveform == xclemulation::debug_mode::gui)
      {
        // NOTE: proto inst filename must match name in HPIKernelCompilerHwEmu.cpp
        std::string protoFileName = "./" + bdName + "_behav.protoinst";
        std::stringstream cmdLineOption;
        std::string waveformDebugfilePath = "";
        sim_path = binaryDirectory + "/behav_waveform/" + simulatorType;
        setSimPath(sim_path);

        if (boost::filesystem::exists(sim_path) != false) {
          waveformDebugfilePath = sim_path + "/waveform_debug_enable.txt";
          if (simulatorType == "xsim") {
            cmdLineOption << " -g --wdb " << wdbFileName << ".wdb"
                          << " --protoinst " << protoFileName;
            launcherArgs = launcherArgs + cmdLineOption.str();
          } else {
            cmdLineOption << " gui ";
            launcherArgs = launcherArgs + cmdLineOption.str();
          }
        }

        std::string generatedWcfgFileName = sim_path + "/" + bdName + "_behav.wcfg";
        unsetenv("VITIS_LAUNCH_WAVEFORM_BATCH");
        if (waveformDebugfilePath != "" && boost::filesystem::exists(waveformDebugfilePath) != false) {
          setenv("VITIS_WAVEFORM", generatedWcfgFileName.c_str(), true);
          setenv("VITIS_WAVEFORM_WDB_FILENAME", std::string(wdbFileName + ".wdb").c_str(), true);
        } else {
          std::string dMsg = "WARNING: [HW-EMU 08-1] None of the Kernels compiled in the waveform enabled mode to get the WDB file. Do run V++ link with the -g option";
          logMessage(dMsg, 0);
        }
        setenv("VITIS_KERNEL_PROFILE_FILENAME", kernelProfileFileName.c_str(), true);
        setenv("VITIS_KERNEL_TRACE_FILENAME", kernelTraceFileName.c_str(), true);

        const char* ldisplay = std::getenv("DISPLAY");
        if (!ldisplay)
        {
          if (mLogStream.is_open())
          {
            mLogStream << __func__ << " DISPLAY environment is not available so expect an exit from the application " << std::endl;
            //DEBUG_MSGS_COUT(" DISPLAY environment is not available so expect an exit from the application ");
          }
          std::string dMsg = "ERROR: [HW-EMU 26] DISPLAY environment is not available so expect an exit from the application";
          logMessage(dMsg, 0);
          throw std::runtime_error(" Simulator did not start/exited, please simulate.log in .run directory!");
        }
        
      }

      if (lWaveform == xclemulation::debug_mode::batch)
      {
        // NOTE: proto inst filename must match name in HPIKernelCompilerHwEmu.cpp
        std::string protoFileName = "./" + bdName + "_behav.protoinst";
        std::stringstream cmdLineOption;

        cmdLineOption << " --wdb " << wdbFileName << ".wdb"
          << " --protoinst " << protoFileName;

        launcherArgs = launcherArgs + cmdLineOption.str();
        sim_path = binaryDirectory + "/behav_waveform/" + simulatorType;
        setSimPath(sim_path);
        std::string waveformDebugfilePath = sim_path + "/waveform_debug_enable.txt";

        std::string generatedWcfgFileName = sim_path + "/" + bdName + "_behav.wcfg";
        setenv("VITIS_LAUNCH_WAVEFORM_BATCH", "1", true);
        if (boost::filesystem::exists(waveformDebugfilePath) != false) {
          setenv("VITIS_WAVEFORM", generatedWcfgFileName.c_str(), true);
          setenv("VITIS_WAVEFORM_WDB_FILENAME", std::string(wdbFileName + ".wdb").c_str(), true);
        } else {
          std::string dMsg = "WARNING: [HW-EMU 08-2] None of the Kernels compiled in the waveform enabled mode to get the WDB file. Do run v++ link with the -g option";
          logMessage(dMsg, 0);
        }
        setenv("VITIS_KERNEL_PROFILE_FILENAME", kernelProfileFileName.c_str(), true);
        setenv("VITIS_KERNEL_TRACE_FILENAME", kernelTraceFileName.c_str(), true);
      }

      if (lWaveform == xclemulation::debug_mode::off) {
        // NOTE: proto inst filename must match name in HPIKernelCompilerHwEmu.cpp
        std::string protoFileName = "./" + bdName + "_behav.protoinst";
        std::stringstream cmdLineOption;

        cmdLineOption << " --wdb " << wdbFileName << ".wdb"
          << " --protoinst " << protoFileName;

        launcherArgs = launcherArgs + cmdLineOption.str();
        sim_path = binaryDirectory + "/behav_waveform/" + simulatorType;
        setSimPath(sim_path);
        setenv("VITIS_LAUNCH_WAVEFORM_BATCH", "1", true);
      }

      /*if (lWaveform == xclemulation::debug_mode::gdb) {
        sim_path = binaryDirectory + "/behav_gdb/" + simulatorType;
        setSimPath(sim_path);
      }*/

      if (userSpecifiedSimPath.empty() == false)
      {
        sim_path = userSpecifiedSimPath;
        setSimPath(sim_path);
        systemUtil::makeSystemCall(sim_path, systemUtil::systemOperation::PERMISSIONS, "777", std::to_string(__LINE__));
      }
      else
      {
        if (sim_path.empty())
        {
          sim_path = binaryDirectory + "/behav_waveform/" + simulatorType;
          setSimPath(sim_path);
        }

        // As gdb feature is unsupported for 2021.1, we removed this cross check. We will re-enable it once we have 2 possibilities
        /*if (boost::filesystem::exists(sim_path) == false)
        {
          if (lWaveform == xclemulation::debug_mode::gdb) {
            sim_path = binaryDirectory + "/behav_waveform/" + simulatorType;
            setSimPath(sim_path);
            std::string waveformDebugfilePath = sim_path + "/waveform_debug_enable.txt";

            std::string dMsg = "WARNING: [HW-EMU 07] debug_mode is set to 'gdb' in INI file and none of kernels compiled in 'gdb' mode. Running simulation using waveform mode. Do run v++ link with -g and --xp param:hw_emu.debugMode=gdb options to launch simulation in 'gdb' mode";
            logMessage(dMsg, 0);

            std::string protoFileName = "./" + bdName + "_behav.protoinst";
            std::stringstream cmdLineOption;
            cmdLineOption << " --wdb " << wdbFileName << ".wdb"
              << " --protoinst " << protoFileName;

            launcherArgs = launcherArgs + cmdLineOption.str();
            std::string generatedWcfgFileName = sim_path + "/" + bdName + "_behav.wcfg";
            setenv("VITIS_LAUNCH_WAVEFORM_BATCH", "1", true);
            if (boost::filesystem::exists(waveformDebugfilePath) != false) {
              setenv("VITIS_WAVEFORM", generatedWcfgFileName.c_str(), true);
              setenv("VITIS_WAVEFORM_WDB_FILENAME", std::string(wdbFileName + ".wdb").c_str(), true);
            }

            // Commented to set these when debug_mode is set to gdb
            //setenv("VITIS_KERNEL_PROFILE_FILENAME", kernelProfileFileName.c_str(), true);
            //setenv("VITIS_KERNEL_TRACE_FILENAME", kernelTraceFileName.c_str(), true);
          }
          else {
            std::string dMsg;
            sim_path = binaryDirectory + "/behav_gdb/" + simulatorType;
            setSimPath(sim_path);
            if (lWaveform == xclemulation::debug_mode::gui)
              dMsg = "WARNING: [HW-EMU 07] debug_mode is set to 'gui' in ini file. Cannot enable simulator gui in this mode. Using " + sim_path + " as simulation directory.";
            else if (lWaveform == xclemulation::debug_mode::batch)
              dMsg = "WARNING: [HW-EMU 07] debug_mode is set to 'batch' in ini file. Using " + sim_path + " as simulation directory.";
            else
              dMsg = "WARNING: [HW-EMU 07] debug_mode is set to 'off' in ini file (or) considered by default. Using " + sim_path + " as simulation directory.";

            logMessage(dMsg, 0);
          }
        }*/
      }

      if (mLogStream.is_open())
        mLogStream << __func__ << " Preparing the launcher args and construction of proper simpath is complete sim_path: " << sim_path << std::endl;

      std::stringstream socket_id;
      socket_id << deviceName << "_" << binaryCounter << "_";
#ifndef _WINDOWS
      // TODO: Windows build support
      //   getpid is defined in unistd.h
      //   setenv is defined in stdlib.h
      socket_id << getpid();
      setenv("EMULATION_SOCKETID", socket_id.str().c_str(), true);
#endif
      if (mLogStream.is_open())
        mLogStream << __func__ << " socket_id: " << socket_id.str().c_str() << " binaryCounter: " << binaryCounter << std::endl;

      binaryCounter++;
    }

    if(mHostMemAccessThreadStarted == false) {
      mHostMemAccessThread = std::thread([this]() { hostMemAccessThread(); } );
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
    if (boost::filesystem::exists(sim_path) == true) {
#ifndef _WINDOWS
      // TODO: Windows build support
      //   pid_t, fork, chdir, execl is defined in unistd.h
      //   this environment variable is added to disable the systemc copyright message

      std::string qemu_dtb, pmc_dtb;
      if (args.m_emuData)
      {
        extractEmuData(sim_path, binaryCounter, args);
        nocMmapInitialization(sim_path);

        std::string emu_data_path = sim_path + "/emulation_data";
        getDtbs(emu_data_path, qemu_dtb, pmc_dtb);
      }

      setenv("SYSTEMC_DISABLE_COPYRIGHT_MESSAGE", "1", true);
      std::cout << std::flush;
      pid_t pid = fork();
      assert(pid >= 0);

      if (mLogStream.is_open())
        mLogStream << __func__ << " Child process created to launch the simulation process " << std::endl;

      if (pid == 0) { //I am child

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

        std::string userSpecifiedPreSimScript = xclemulation::config::getInstance()->getUserPreSimScript();
        std::string userSpecifiedPostSimScript = xclemulation::config::getInstance()->getUserPostSimScript();
        std::string wcfgFilePath = xclemulation::config::getInstance()->getWcfgFilePath();

        if (userSpecifiedPreSimScript != "" && wcfgFilePath != "") {
          std::cout << "WARNING: [HW-EMU] Both user_pre_sim_script and wcfg_file_path are provided. Either one of the option is accepted. Giving predence for wcfg_file_path." << std::endl;
        }

        std::string pre_sim_script;
        if (wcfgFilePath != "") {
          createPreSimScript(wcfgFilePath, pre_sim_script);
        }

        if (args.m_emuData) {

          //Assuming that we will have only one AIE Kernel, need to
          //update this logic when we have suport for multiple AIE Kernels

          if (fs::exists(sim_path + "/emulation_data/libsdf/cfg/aie.sim.config.txt"))
          {
            launcherArgs += " -emuData " + sim_path + "/emulation_data/libsdf/cfg/aie.sim.config.txt";
            launcherArgs += " -aie-sim-config " + sim_path + "/emulation_data/libsdf/cfg/aie.sim.config.txt";
          }
          else if (fs::exists(sim_path + "/emulation_data/libadf/cfg/aie.sim.config.txt")) {
            launcherArgs += " -emuData " + sim_path + "/emulation_data/libadf/cfg/aie.sim.config.txt";
            launcherArgs += " -aie-sim-config " + sim_path + "/emulation_data/libadf/cfg/aie.sim.config.txt";
          } else {
            launcherArgs += " -emuData " + sim_path + "/emulation_data/cfg/aie.sim.config.txt";
            launcherArgs += " -aie-sim-config " + sim_path + "/emulation_data/cfg/aie.sim.config.txt";
          }

          if (fs::exists(sim_path + "/emulation_data/BOOT_bh.bin")) {
            launcherArgs += " -boot-bh " + sim_path + "/emulation_data/BOOT_bh.bin";
          }

          if (fs::exists(sim_path + "/emulation_data/qemu_ospi.bin")) {
            launcherArgs += " -ospi-image " + sim_path + "/emulation_data/qemu_ospi.bin";
          }

          if (fs::exists(sim_path + "/emulation_data/qemu_qspi_low.bin")) {
            launcherArgs += " -qspi-low-image " + sim_path + "/emulation_data/qemu_qspi_low.bin";
          }

          if (fs::exists(sim_path + "/emulation_data/qemu_qspi_high.bin")) {
            launcherArgs += " -qspi-high-image " + sim_path + "/emulation_data/qemu_qspi_high.bin";
          }

          // V70 support: Setting this option, launch_emulator does not set the NOCSIM_DRAM_FILE file, it auto sets the
          // NOCSIM_MULTI_DRAM_FILE
          if (fs::exists(sim_path + "/emulation_data/noc_memory_config.txt")) {
            launcherArgs += " -noc-memory-config " + sim_path + "/emulation_data/noc_memory_config.txt";
          }

          if (fs::exists(sim_path + "/emulation_data/qemu_args.txt")) {
            launcherArgs += " -qemu-args-file " + sim_path + "/emulation_data/qemu_args.txt";
          }

          if (fs::exists(sim_path + "/emulation_data/pmc_args.txt")) {
            launcherArgs += " -pmc-args-file " + sim_path + "/emulation_data/pmc_args.txt";
          }
          else if (fs::exists(sim_path + "/emulation_data/pmu_args.txt")) {
            launcherArgs += " -pmc-args-file " + sim_path + "/emulation_data/pmu_args.txt";
          }
          else {
            std::cout << "ERROR: [HW-EMU] Unable to find either PMU/PMC args which are required to launch the emulation." << std::endl;
          }

          if (!qemu_dtb.empty())
            launcherArgs += " -qemu-dtb " + qemu_dtb;
        
          if (!pmc_dtb.empty())
            launcherArgs += " -pmc-dtb  " + pmc_dtb;

          if (is_enable_debug) {
            launcherArgs += " -enable-debug ";
          }

          if (aie_sim_options != "") {
            launcherArgs += " -aie-sim-options " + aie_sim_options;
          }

          if (wcfgFilePath != "") {
            launcherArgs += " -user-pre-sim-script " + pre_sim_script;
          }
          else {
            if (userSpecifiedPreSimScript != "") {
              launcherArgs += " -user-pre-sim-script " + userSpecifiedPreSimScript;
            }
          }

          if (userSpecifiedPostSimScript != "") {
            launcherArgs += " -user-post-sim-script " + userSpecifiedPostSimScript;
          }
        }
        else {
          if (pre_sim_script != "" && wcfgFilePath != "") {
            setenv("USER_PRE_SIM_SCRIPT", pre_sim_script.c_str(), true);
          }
        }

        if (!launcherArgs.empty())
          simMode = launcherArgs.c_str();

        //if (!xclemulation::file_exists(sim_file))
        if (!boost::filesystem::exists(sim_file))
          sim_file = "simulate.sh";
          
        if (mLogStream.is_open() )
            mLogStream << __TIME__ <<"\t"<< __func__ << " The simulate script is "  <<sim_file  << std::endl;

        int r = execl(sim_file.c_str(), sim_file.c_str(), simMode, NULL);
        if (r == -1) {
          std::cerr << "FATAL ERROR : Simulation process did not launch" << std::endl; 
          exit(1);
        } 
        exit(0);
      }
#endif
    }

    if (mLogStream.is_open())
      mLogStream << __func__ << " Child process launched... " << std::endl;
    //if platform is a XPR platform, dont serilize ddr memory
    if (isXPR())
    {
      mEnvironmentNameValueMap["enable_pr"] = "false";
    }
    //Providing enough time to simulate script to set up it's environment and start xsim.
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10s);
    if (parseLog() != 0) {
      if (mLogStream.is_open())
        mLogStream << __func__ << " ERROR: [HW-EMU 26] Simulator is NOT started so exiting the application! " << std::endl;
        
      // If no simulator running then no need to try a connection, hence exit with a failure now.
      //throw std::runtime_error(" Simulator did not start/exited, please refer simulate.log in .run directory!");
      exit(EXIT_FAILURE);
    }

    std::string simulationDirectoryMsg = "INFO: [HW-EMU 05] Path of the simulation directory : " + getSimPath();
    logMessage(simulationDirectoryMsg);

    sock = std::make_shared<unix_socket>();
    set_simulator_started(true);
    sock->monitor_socket();
    
    //Thread to fetch messages from Device to display on host
    if (mMessengerThreadStarted == false) {
      mMessengerThread = std::thread([this]() { messagesThread(); } );
      mMessengerThreadStarted = true;
    }

    if (mLogStream.is_open())
      mLogStream << __func__ << " mMessengerThreadStarted " << std::endl;

    if (mLogStream.is_open())
      mLogStream << __func__ << " Created the Unix socket." << std::endl;

    if (sock && mEnvironmentNameValueMap.empty() == false)
    {
      //send environment information to device
      bool ack = true;

      if (mLogStream.is_open())
        mLogStream << __func__ << " Before RPC call xclSetEnvironment_RPC_CALL." << std::endl;

      xclSetEnvironment_RPC_CALL(xclSetEnvironment);
      if (!ack)
      {
        if (mLogStream.is_open())
          mLogStream << __func__ << "Environment is NOT set properly" << std::endl;
        //std::cout<<"environment is not set properly"<<std::endl;
      }

      if (mLogStream.is_open())
        mLogStream << __func__ << "Environment is set properly" << std::endl;
    }

    return 0;
  }

  bool HwEmShim::readEmuSettingsJsonFile(const std::string& emuSettingsFilePath) {

    if (emuSettingsFilePath.empty() || !boost::filesystem::exists(emuSettingsFilePath)) {
      return false;
    }

    try
    {
      std::ifstream emuSettingsFile(emuSettingsFilePath.c_str());
      if (!emuSettingsFile.good()) {
        return false;
      }

      namespace pt = boost::property_tree;
      pt::ptree iroot;
      pt::read_json(emuSettingsFilePath, iroot);

      for (pt::ptree::value_type &iter : iroot.get_child("settings"))
      {
        std::string settingName = iter.first;
        std::string settingValue = iter.second.get_value<std::string>();
        if (settingName == "defer_device_process") {
          mSimDontRun = (settingValue == "true") ? true : false;
        }
      }
    }
    catch (const boost::property_tree::ptree_error &e)
    {
      std::cerr << "property_tree error = " << e.what() << std::endl;
      return false;
    }
    catch (std::exception const& e)
    {
      std::cerr << "exception = " << e.what() << std::endl;
      return false;
    }
    return true;
  }

  void HwEmShim::createPreSimScript(const std::string& wcfgFilePath, std::string& preSimScriptPath) {
    char path[FILENAME_MAX];
    size_t size = MAXPATHLEN;
    char* pPath = GetCurrentDir(path, size);

    preSimScriptPath = std::string(pPath) + "/pre_sim_script.tcl";
    std::ofstream pssStrem;
    pssStrem.open(preSimScriptPath);

    pssStrem << "open_wave_config " << wcfgFilePath << std::endl;
    pssStrem.close();
  }

  void HwEmShim::nocMmapInitialization(const std::string &simPath)
  {
    if (xclemulation::config::getInstance()->isFastNocDDRAccessEnabled())
    {
      std::string nocMemSpecFilePath = simPath + "/emulation_data/noc_memory_config.txt";
      if (fs::exists(nocMemSpecFilePath))
        this->mNocFastAccess.init(nocMemSpecFilePath, simPath);
    }
  }

  void HwEmShim::extractEmuData(const std::string &simPath, int binaryCounter, bitStreamArg args)
  {

    std::unique_ptr<char[]> emuDataFileName(new char[1024]);
#ifndef _WINDOWS
    // TODO: Windows build support
    // getpid is defined in unistd.h
    std::sprintf(emuDataFileName.get(), "%s/emuDataFile_%d", mRunDeviceBinDir.c_str(), binaryCounter);
#endif

    if ((args.m_emuData != nullptr) && (args.m_emuDataSize > 1))
    {
      std::ofstream os(emuDataFileName.get());
      os.write(args.m_emuData, args.m_emuDataSize);
      os.close();

      std::string emuDataFilePath(emuDataFileName.get());
      systemUtil::makeSystemCall(emuDataFilePath, systemUtil::systemOperation::UNZIP, simPath, std::to_string(__LINE__));
      systemUtil::makeSystemCall(mRunDeviceBinDir, systemUtil::systemOperation::PERMISSIONS, "777", std::to_string(__LINE__));
    }
  }

  void HwEmShim::getDtbs(const std::string& emu_data_path, std::string& qemu_dtb, std::string& pmc_dtb) 
  {
    boost::filesystem::path dts_dir = emu_data_path;
    boost::filesystem::directory_iterator end_itr;

    for (boost::filesystem::directory_iterator itr(dts_dir); itr != end_itr; ++itr)
    {
      std::string current_file = itr->path().string();
      std::string file_str = itr->path().filename().string();

      if (boost::algorithm::ends_with(file_str, ".dtb") == true)
      {
        if (mVersalPlatform)
        {
          if (file_str.find("pmc-virt") != std::string::npos)
            pmc_dtb = emu_data_path + "/" + file_str;
          else
            qemu_dtb = emu_data_path + "/" + file_str;
        }
        else
        {
          if (file_str.find("pmu.dtb") != std::string::npos)
            pmc_dtb = emu_data_path + "/" + file_str;
          else
            qemu_dtb = emu_data_path + "/" + file_str;
        }
      }
    }
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
           //Note: Adding PF and BAR ID valuesas 0, Once original values are avaiaable they get replaced
           xclWriteAddrSpaceDeviceRam_RPC_CALL(xclWriteAddrSpaceDeviceRam ,space,offset,curr,size,0,0);
           PRINTENDFUNC;
           return totalSize;
         }
       case XCL_ADDR_SPACE_DEVICE_PERFMON:
         {
           const char *curr = (const char *)hostBuf;
           std::map<uint64_t,std::pair<std::string,unsigned int>> offsetArgInfo;
           //Note: Adding PF and BAR ID valuesas 0, Once original values are avaiaable they get replaced
           xclWriteAddrKernelCtrl_RPC_CALL(xclWriteAddrKernelCtrl ,space,offset,curr,size,offsetArgInfo,0,0);
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

           std::string kernelName("");
           uint32_t *hostBuf32 = ((uint32_t*)hostBuf);
          // if(hostBuf32[0] & CONTROL_AP_START)

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
             std::string dMsg ="INFO: [HW-EMU 04-0] Sending start signal to the kernel " + kernelName;
             logMessage(dMsg,1);
           }
           else
           {
             std::string dMsg ="INFO: [HW-EMU 03-0] Configuring registers for the kernel " + kernelName +" Started";
             logMessage(dMsg,1);
           }
           //Note: Adding PF and BAR ID valuesas 0, Once original values are avaiaable they get replaced
           xclWriteAddrKernelCtrl_RPC_CALL(xclWriteAddrKernelCtrl,space,offset,hostBuf,size,offsetArgInfo,0,0);
           if(hostBuf32[0] & CONTROL_AP_START)
           {
             std::string dMsg ="INFO: [HW-EMU 04-1] Kernel " + kernelName +" is Started";
             logMessage(dMsg,1);
           }
           else
           {
             std::string dMsg ="INFO: [HW-EMU 03-1] Configuring registers for the kernel " + kernelName +" Ended";
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
          //Note: Adding PF and BAR ID valuesas 0, Once original values are avaiaable they get replaced
          xclReadAddrSpaceDeviceRam_RPC_CALL(xclReadAddrSpaceDeviceRam,space,offset,hostBuf,size,0,0);
          PRINTENDFUNC;
          return totalSize;
        }
      case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
          xclGetDebugMessages();
          //Note: Adding PF and BAR ID valuesas 0, Once original values are avaiaable they get replaced
          xclReadAddrKernelCtrl_RPC_CALL(xclReadAddrKernelCtrl,space,offset,hostBuf,size,0,0);
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
          //Note: Adding PF and BAR ID valuesas 0, Once original values are avaiaable they get replaced
          xclReadAddrKernelCtrl_RPC_CALL(xclReadAddrKernelCtrl,space,offset,hostBuf,size,0,0);
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

  uint32_t HwEmShim::getAddressSpace(uint32_t topology)
  {
    if (mMembanks.size() <= topology)
      return 0;
    if (mMembanks[topology].tag.find("bank") != std::string::npos)
    {
      return 0;
    }
    if (mMembanks[topology].tag.find("HBM") != std::string::npos)
    {
      return 2;
    }
    return 1;
  }

  size_t HwEmShim::xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek, uint32_t topology)
  {
    if (!sock)
    {
      if (!mMemModel)
        mMemModel = new mem_model(deviceName);
      mMemModel->writeDevMem(dest, src, size);
      return size;
    }
    std::string dMsg;
    //TODO : workaround to access NOC DDR in a faster way
    //Need to be removed when NOC allows transaport_dbg access

    if (xclemulation::config::getInstance()->isFastNocDDRAccessEnabled())
    {
      std::cout << "Checking Write Fastmem " << dest << std::endl;
      if (mNocFastAccess.isAddressMapped(dest, size))
      {
        std::cout << "Writing Fastmem " << dest << std::endl;
        mNocFastAccess.write(dest, (unsigned char *)src, size);
        dMsg = "INFO: [HW-EMU 02-1] Copying buffer from host to device ended";
        logMessage(dMsg, 1);
        PRINTENDFUNC;
        printMem(mGlobalInMemStream, 16, dest, (void *)src, size);
        return size;
      }
    }

    src = (unsigned char *)src + seek;
    if (mLogStream.is_open())
    {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
                 << src << ", " << size << ", " << seek << std::endl;
    }
    dMsg = "INFO: [HW-EMU 02-0] Copying buffer from host to device started : size = " + std::to_string(size);
    logMessage(dMsg, 1);
    void *handle = this;

    uint64_t messageSize = xclemulation::config::getInstance()->getPacketSize();
    uint64_t c_size = messageSize;
    uint64_t processed_bytes = 0;
    while (processed_bytes < size)
    {
      if ((size - processed_bytes) < messageSize)
      {
        c_size = size - processed_bytes;
      }
      else
      {
        c_size = messageSize;
      }

      void *c_src = (((unsigned char *)(src)) + processed_bytes);
      uint64_t c_dest = dest + processed_bytes;
#ifndef _WINDOWS
      // TODO: Windows build support
      // *_RPC_CALL uses unix_socket
      uint32_t space = getAddressSpace(topology);
      xclCopyBufferHost2Device_RPC_CALL(xclCopyBufferHost2Device, handle, c_dest, c_src, c_size, seek, space);
#endif
      processed_bytes += c_size;
    }
    dMsg = "INFO: [HW-EMU 02-1] Copying buffer from host to device ended";
    logMessage(dMsg, 1);

    PRINTENDFUNC;
    printMem(mGlobalInMemStream, 16, dest, (void *)src, size);

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

    std::string dMsg = "INFO: [HW-EMU 05-0] Copying buffer from device to host started. size := " + std::to_string(size);
    logMessage(dMsg, 1);
    void *handle = this;

    //TODO : workaround to access NOC DDR in a faster way
    //Need to be removed when NOC allows transaport_dbg access
    if (xclemulation::config::getInstance()->isFastNocDDRAccessEnabled())
    {
      std::cout << "Checking Read Fastmem " << src << std::endl;
      if (mNocFastAccess.isAddressMapped(src, size))
      {
        mNocFastAccess.read(src, (unsigned char *)dest, size);
        std::cout << "Reading Fastmem " << src << std::endl;
        dMsg = "INFO: [HW-EMU 05-1] Copying buffer from device to host ended";
        logMessage(dMsg, 1);
        PRINTENDFUNC;
        return size;
      }
    }

    uint64_t messageSize = xclemulation::config::getInstance()->getPacketSize();
    uint64_t c_size = messageSize;
    uint64_t processed_bytes = 0;

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
    dMsg ="INFO: [HW-EMU 05-1] Copying buffer from device to host ended";
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
      bool noHostMemory= false;
      std::string sFileName("");
      xclAllocDeviceBuffer_RPC_CALL(xclAllocDeviceBuffer,finalValidAddress,origSize,noHostMemory);

      PRINTENDFUNC;
      if(!ack)
        return 0;
    }
    return finalValidAddress;
  }

  uint64_t HwEmShim::xclAllocDeviceBuffer2(size_t& size, xclMemoryDomains domain, unsigned flags, bool noHostMemory, unsigned boFlags, std::string &sFileName, std::map<uint64_t,uint64_t>& chunks)
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
    uint64_t result = -1;

    if (boFlags & XCL_BO_FLAGS_HOST_ONLY) {
      result = mDDRMemoryManager[host_sptag_idx]->alloc(size, paddingFactor, chunks);
    }
    else {
      result = mDDRMemoryManager[flags]->alloc(size, paddingFactor, chunks);
    }

    if (result == xclemulation::MemoryManager::mNull) {
      return result;
    }

    uint64_t finalValidAddress = result+(paddingFactor*size);
    uint64_t finalSize = size+(2*paddingFactor*size);
    mAddrMap[finalValidAddress] = finalSize;
    bool ack = false;
    if (sock)
    {
      if (boFlags & XCL_BO_FLAGS_HOST_ONLY) { // bypassed the xclAllocDeviceBuffer RPC call for Slave Bridge (host only buffer)
      } else {
	      if(chunks.size())
	      {
		      for (auto it:chunks)
		      {
			      xclAllocDeviceBuffer_RPC_CALL(xclAllocDeviceBuffer, it.first, it.second, noHostMemory);
		      }

	      }
	      else
	      {
		      xclAllocDeviceBuffer_RPC_CALL(xclAllocDeviceBuffer, finalValidAddress, origSize, noHostMemory);
	      }
        PRINTENDFUNC;
        if (!ack)
          return 0;
      }
    }

    return finalValidAddress;
  }

  void HwEmShim::xclFreeDeviceBuffer(uint64_t offset, bool sendtoxsim)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << offset << std::endl;
    }

    for (auto i : mDDRMemoryManager) {
      if (offset < i->start() + i->size()) {
        i->free(offset);
      }
    }
    bool ack = true;
    if(sock)
    {
      //Currently Versal platforms does not support buffer deallocation
      if(!mVersalPlatform && sendtoxsim) {
        xclFreeDeviceBuffer_RPC_CALL(xclFreeDeviceBuffer,offset);
      }
    }
    if(!ack)
    {
      PRINTENDFUNC;
      return;
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

     xclemulation::debug_mode lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();

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

      if(pPath) {
        // Copy waveform database
        if (lWaveform != xclemulation::debug_mode::off) {
          std::string extension = "wdb";
          if (boost::filesystem::exists(binaryDirectory+"/msim")) {
            extension = "wlf";
          }
          std::string wdbFileName = binaryDirectory + "/" + fileName + "."+extension;
          std::string destPath = "'" + std::string(path) + "/" + fileName +"." + extension + "'";
          systemUtil::makeSystemCall(wdbFileName, systemUtil::systemOperation::COPY,destPath, std::to_string(__LINE__));

          // Copy waveform config
          std::string wcfgFilePath= binaryDirectory + "/" + bdName + "_behav.wcfg";
          std::string destPath2 = "'" + std::string(path) + "/" + fileName + ".wcfg'";
          systemUtil::makeSystemCall(wcfgFilePath, systemUtil::systemOperation::COPY, destPath2, std::to_string(__LINE__));

          // Append to detailed kernel trace data mining results file
          std::string logFilePath= binaryDirectory + "/profile_kernels.csv";
          std::string destPath3 = "'" + std::string(path) + "/profile_kernels.csv'";
          systemUtil::makeSystemCall(logFilePath, systemUtil::systemOperation::APPEND, destPath3, std::to_string(__LINE__));
          xclemulation::copyLogsFromOneFileToAnother(logFilePath, mDebugLogStream);

          // Append to detailed kernel trace "timeline" file
          std::string traceFilePath = binaryDirectory + "/timeline_kernels.csv";
          std::string destPath4 = "'" + std::string(path) + "/timeline_kernels.csv'";
          systemUtil::makeSystemCall(traceFilePath, systemUtil::systemOperation::APPEND, destPath4, std::to_string(__LINE__));

          // Copy proto inst file
          std::string protoFilePath= binaryDirectory + "/" + bdName + "_behav.protoinst";
          std::string destPath6 = "'" + std::string(path) + "/" + fileName + ".protoinst'";
          systemUtil::makeSystemCall(protoFilePath, systemUtil::systemOperation::COPY, destPath6, std::to_string(__LINE__));


          if (mLogStream.is_open())
            mLogStream << "appended " << logFilePath << " to " << destPath3 << std::endl;
        }

        // Copy Simulation Log file
        std::string simulationLogFilePath= binaryDirectory + "/" + "simulate.log";
        std::string destPath5 = "'" + std::string(path) + "/" + fileName + "_simulate.log'";
        systemUtil::makeSystemCall(simulationLogFilePath, systemUtil::systemOperation::COPY, destPath5, std::to_string(__LINE__));

        // Copy xsc_report Log file
        std::string xscReportLogFilePath= binaryDirectory + "/" + "xsc_report.log";
        std::string destPath8 = "'" + std::string(path) + "/" + fileName + "_xsc_report.log'";
        systemUtil::makeSystemCall(xscReportLogFilePath, systemUtil::systemOperation::COPY, destPath8, std::to_string(__LINE__));
      }
      i++;
    }
    mBinaryDirectories.clear();
    PRINTENDFUNC;

    if (mLogStream.is_open()) {
      mLogStream.close();
    }
  }

  void HwEmShim::xclClose(bool DonotRunParseLog ) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

    // Ensuring parseLog call to xclClose not to call parseLog again. - no recursive call should happen.
    if (!DonotRunParseLog)
        parseLog();

    for (auto& it: mFdToFileNameMap) {
      int fd=it.first;
      int sSize = std::get<1>(it.second);
      void* addr = std::get<2>(it.second);
      munmap(addr,sSize);
      close(fd);
    }
    mFdToFileNameMap.clear();

    // Shim object is not deleted as part of closing device.
    // The core device must correspond to open and close, so
    // reset here rather than in destructor
    xdp::hw_emu::flush_device(this);
    mCoreDevice.reset();
    device_handles::remove(this);

    if (!sock)
    {
      if (xclemulation::config::getInstance()->isKeepRunDirEnabled() == false) {
        systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::REMOVE, "", std::to_string(__LINE__));
      }

      if(mMBSch && mCore)
      {
        mMBSch->fini_scheduler_thread();
        delete mCore;
        mCore = NULL;
        delete mMBSch;
        mMBSch = NULL;
      }
      if(m_scheduler)
      {
          delete m_scheduler;
          m_scheduler = nullptr;
      }
      if(m_xgq)
      {
          delete m_xgq;
          m_xgq = nullptr;
      }
      PRINTENDFUNC;
      if (mLogStream.is_open()) {
        mLogStream.close();
      }
      return;
    }
    // RPC calls will not be made in resetprogram
    // resetProgram has to be called in xclclose because all running threads will be exited gracefully 
    // which results clean exit of driver code.  
      resetProgram(false);


    int status = 0;
    xclemulation::debug_mode lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();
    if ((lWaveform == xclemulation::debug_mode::gui || lWaveform == xclemulation::debug_mode::batch || lWaveform == xclemulation::debug_mode::off)
      && xclemulation::config::getInstance()->isInfoSuppressed() == false)
    {
      std::string waitingMsg ="INFO: [HW-EMU 06-0] Waiting for the simulator process to exit";
      logMessage(waitingMsg);
    }

    //bool simDontRun = xclemulation::config::getInstance()->isDontRun();
    if (!mSimDontRun)
      while (-1 == waitpid(0, &status, 0));

    if ((lWaveform == xclemulation::debug_mode::gui || lWaveform == xclemulation::debug_mode::batch || lWaveform == xclemulation::debug_mode::off)
      && xclemulation::config::getInstance()->isInfoSuppressed() == false)
    {
      std::string waitingMsg ="INFO: [HW-EMU 06-1] All the simulator processes exited successfully";
      logMessage(waitingMsg);

      std::string consoleMsg = "INFO: [HW-EMU 07-0] Please refer the path \"" + getSimPath() + "/simulate.log\" for more detailed simulation infos, errors and warnings.";
      logMessage(consoleMsg);
    }

    saveWaveDataBase();
    if (xclemulation::config::getInstance()->isKeepRunDirEnabled() == false)
      systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::REMOVE, "", std::to_string(__LINE__));
    google::protobuf::ShutdownProtobufLibrary();
    PRINTENDFUNC;
    if (mLogStream.is_open()) {
      mLogStream.close();
    }
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
      if(m_scheduler)
      {
          delete m_scheduler;
          m_scheduler = nullptr;
      }
      if(m_xgq)
      {
          delete m_xgq;
          m_xgq = nullptr;
      }
      return 0;
    }

#ifndef _WINDOWS
    // TODO: Windows build support
    // *_RPC_CALL uses unix_socket
#endif
    Event eventObj;
    uint32_t numSlots = getPerfMonNumberSlots(xdp::MonitorType::memory);
    bool ack = true;
    for(unsigned int counter = 0 ; counter < numSlots; counter++)
    {
      unsigned int samplessize = 0;

      // This used to compare to XPAR_AIM0_HOST_SLOT, which is a meaningless
      // definition now.  Changing it to the equivalent 0 for now, but this
      // section should be revisited.
      if (counter == 0)
        continue;

      char slotname[128];
      getPerfMonSlotName(xdp::MonitorType::memory,counter,slotname,128);

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
    try {
      std::lock_guard<std::mutex> guard(mPrintMessagesLock);
      simulator_started = false;
      fetchAndPrintMessages();

    }
    catch (std::exception& ex) {
      if (mLogStream.is_open())
        mLogStream << __func__ << ", unable to get lock:: " <<ex.what()<< std::endl;

      std::cout<<"\n unable to get lock::"<<ex.what();
    }

    std::string socketName = sock->get_name();
    if(socketName.empty() == false)// device is active if socketName is non-empty
    {
#ifndef _WINDOWS
      xclClose_RPC_CALL(xclClose,this);
#endif
      closeMessengerThread();
      //clean up directories which are created inside the driver
      systemUtil::makeSystemCall(socketName, systemUtil::systemOperation::REMOVE, "", std::to_string(__LINE__));
    }

    if(saveWdb)
    {
      int status = 0;
      xclemulation::debug_mode lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();
      if(( lWaveform == xclemulation::debug_mode::gui || lWaveform == xclemulation::debug_mode::batch || lWaveform == xclemulation::debug_mode::off )
        && xclemulation::config::getInstance()->isInfoSuppressed() == false)
      {
        std::string waitingMsg ="INFO: [HW-EMU 06-0] Waiting for the simulator process to exit";
        logMessage(waitingMsg);
      }

      //bool simDontRun = xclemulation::config::getInstance()->isDontRun();
      if (!mSimDontRun)
        while (-1 == waitpid(0, &status, 0));

      if(( lWaveform == xclemulation::debug_mode::gui || lWaveform == xclemulation::debug_mode::batch || lWaveform == xclemulation::debug_mode::off )
        && xclemulation::config::getInstance()->isInfoSuppressed() == false)
      {
        std::string waitingMsg ="INFO: [HW-EMU 06-1] All the simulator processes exited successfully";
        logMessage(waitingMsg);
      }

      saveWaveDataBase();
    }
    //ProfilerStop();

    sock.reset();

    PRINTENDFUNC;
    if(mMBSch && mCore)
    {
      mMBSch->fini_scheduler_thread();
      delete mCore;
      mCore = NULL;
      delete mMBSch;
      mMBSch = NULL;
    }
    if(m_scheduler)
    {
        delete m_scheduler;
        m_scheduler = nullptr;
    }
    if(m_xgq)
    {
        delete m_xgq;
        m_xgq = nullptr;
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
    parsedMsgs.clear();

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
    if(m_scheduler)
    {
        delete m_scheduler;
        m_scheduler = nullptr;
    }
    if(m_xgq)
    {
        delete m_xgq;
        m_xgq = nullptr;
    }
    if(mDataSpace)
    {
      delete mDataSpace;
      mDataSpace = NULL;
    }
    closeMessengerThread();
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

  std::string HwEmShim::getSimulatorType(const std::string& binaryDirectory) {

    std::string simulator;
    std::string sim_path1 = binaryDirectory + "/behav_waveform/xsim";
    std::string sim_path2 = binaryDirectory + "/behav_gdb/xsim";

    std::string sim_path3 = binaryDirectory + "/behav_waveform/questa";
    std::string sim_path4 = binaryDirectory + "/behav_waveform/xcelium";
    std::string sim_path5 = binaryDirectory + "/behav_waveform/vcs";

    if (boost::filesystem::exists(sim_path1) || boost::filesystem::exists(sim_path2)) {
      simulator = "xsim";
    }
    else if (boost::filesystem::exists(sim_path3)) {
      simulator = "questa";
    }
    else if (boost::filesystem::exists(sim_path4)) {
      simulator = "xcelium";
    }
    else if (boost::filesystem::exists(sim_path5)) {
      simulator = "vcs";
    }

    if (!boost::filesystem::exists(sim_path1) && !boost::filesystem::exists(sim_path2)
      && !boost::filesystem::exists(sim_path3) && !boost::filesystem::exists(sim_path4)
      && !boost::filesystem::exists(sim_path5)) {

      std::string dMsg = "ERROR: [HW-EMU 11] UNZIP operation failed. Not to able to get the required simulation binaries from xclbin";
      logMessage(dMsg, 0);
    }

    return simulator;
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

  HwEmShim::HwEmShim(unsigned int deviceIndex, xclDeviceInfo2 &info, std::list<xclemulation::DDRBank>& DDRBankList, bool _unified, bool _xpr,
    FeatureRomHeader &fRomHeader, const boost::property_tree::ptree& platformData)
    :mRAMSize(info.mDDRSize)
    ,mCoalesceThreshold(4)
    ,mDeviceIndex(deviceIndex)
    ,mCuIndx(0)
  {
    simulator_started = false;
    tracecount_calls = 0;
    mReqCounter = 0;
    simulatorType = "xsim";
    sim_path = "";
    mSimDontRun = false;

    ci_msg.set_size(0);
    ci_msg.set_xcl_api(0);
#if GOOGLE_PROTOBUF_VERSION < 3006001
    ci_buf = malloc(ci_msg.ByteSize());
#else
    ci_buf = malloc(ci_msg.ByteSizeLong());
#endif
    ri_msg.set_size(0);
#if GOOGLE_PROTOBUF_VERSION < 3006001
    ri_buf = malloc(ri_msg.ByteSize());
#else
    ri_buf = malloc(ri_msg.ByteSizeLong());
#endif

    buf = nullptr;
    buf_size = 0;
    binaryCounter = 0;
    host_sptag_idx = -1;
    sock = nullptr;
    mCURangeMap.clear();

    deviceName = "device"+std::to_string(deviceIndex);
    deviceDirectory = xclemulation::getRunDirectory() +"/" + std::to_string(getpid())+"/hw_em/"+deviceName;

    systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::CREATE, "", std::to_string(__LINE__));
    systemUtil::makeSystemCall(deviceDirectory, systemUtil::systemOperation::PERMISSIONS, "777", std::to_string(__LINE__));

    mPlatformData = platformData;
    constructQueryTable();

    std::memset(&mDeviceInfo, 0, sizeof(xclDeviceInfo2));
    fillDeviceInfo(&mDeviceInfo,&info);
    initMemoryManager(DDRBankList);

    std::memset(&mFeatureRom, 0, sizeof(FeatureRomHeader));
    std::memcpy(&mFeatureRom, &fRomHeader, sizeof(FeatureRomHeader));

    last_clk_time = clock();
    mCloseAll = false;
    mMemModel = nullptr;

    // Delete detailed kernel trace data mining results file
    // NOTE: do this only if we're going to write a new one
    xclemulation::debug_mode lWaveform = xclemulation::config::getInstance()->getLaunchWaveform();
    if (lWaveform == xclemulation::debug_mode::gui
        || lWaveform == xclemulation::debug_mode::batch
        || lWaveform == xclemulation::debug_mode::off) {
      char path[FILENAME_MAX];
      size_t size = MAXPATHLEN;
      char *pPath = GetCurrentDir(path, size);
      if (pPath)
      {
        std::string sdxProfileKernelFile = std::string(path) + "/profile_kernels.csv";
        systemUtil::makeSystemCall(sdxProfileKernelFile, systemUtil::systemOperation::REMOVE, "", std::to_string(__LINE__));
        std::string sdxTraceKernelFile = std::string(path) + "/timeline_kernels.csv";
        systemUtil::makeSystemCall(sdxTraceKernelFile, systemUtil::systemOperation::REMOVE, "", std::to_string(__LINE__));
      }
    }
    bUnified = _unified;
    bXPR = _xpr;
    mCore = nullptr;
    mMBSch = nullptr;
    m_scheduler = nullptr;
    m_xgq = nullptr;
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
    mVersalPlatform=false;
    mHostMemAccessThreadStarted = false;
  }

  bool HwEmShim::isMBSchedulerEnabled()
  {
    if (xclemulation::config::getInstance()->getIsPlatformEnabled()) {
      if (mPlatformData.get_optional<std::string>("plp.ert").is_initialized()) {
        std::string ertStr = mPlatformData.get<std::string>("plp.ert");
        return (ertStr == "enabled" ? true : false);
      }
    }

    bool mbSchEnabled = mFeatureRom.FeatureBitMap & FeatureBitMask::MB_SCHEDULER;
    bool QDMAPlatform = (getDsaVersion() == 60) ? true : false;
    return mbSchEnabled && !QDMAPlatform;
  }

  void HwEmShim::constructQueryTable() {
    if (xclemulation::config::getInstance()->getIsPlatformEnabled()) {

      if (mPlatformData.get_optional<std::string>("plp.m2m").is_initialized()) {
        mQueryTable[key_type::m2m] = mPlatformData.get<std::string>("plp.m2m");
      }

      if (mPlatformData.get_optional<std::string>("plp.dma").is_initialized()) {
        std::string dmaVal = mPlatformData.get<std::string>("plp.dma");
        mQueryTable[key_type::nodma] = (dmaVal == "none" ? "enabled" : "disabled");
      }
    }
  }

  // New API's for device query - m2m and no-dma
  int HwEmShim::deviceQuery(key_type queryKey) {
    if (mQueryTable.find(queryKey) != mQueryTable.end())
      return (mQueryTable[queryKey] == "enabled" ? 1 : 0);
    return 0;
  }

  std::string HwEmShim::getERTVersion() {
    if (xclemulation::config::getInstance()->getIsPlatformEnabled()) {
      if (mPlatformData.get_optional<std::string>("plp.ertVersion").is_initialized()) {
        return (mPlatformData.get<std::string>("plp.ertVersion"));
      }
    }
    return "10";
  }

  uint64_t HwEmShim::getM2MAddress() {
    if (xclemulation::config::getInstance()->getIsPlatformEnabled()) {
      if (mPlatformData.get_optional<std::string>("plp.m2m_address").is_initialized()) {
        std::stringstream streamSS;
        streamSS << std::hex << mPlatformData.get<std::string>("plp.m2m_address");
        uint64_t baseAddr_u;
        streamSS >> baseAddr_u;
        return baseAddr_u;
      }
    }
    return 0;
  }

  uint64_t HwEmShim::getErtCmdQAddress() {

    if (xclemulation::config::getInstance()->getIsPlatformEnabled()) {
      if (mPlatformData.get_optional<std::string>("plp.ertCmdqBaseAddr").is_initialized()) {
        std::stringstream streamSS ;
        streamSS << std::hex << mPlatformData.get<std::string>("plp.ertCmdqBaseAddr");
        uint64_t baseAddr_u;
        streamSS >> baseAddr_u;
        return baseAddr_u;
      }
    }
    return 0;
  }

  uint64_t HwEmShim::getErtBaseAddress() {
    if (xclemulation::config::getInstance()->getIsPlatformEnabled()) {
      if (mPlatformData.get_optional<std::string>("plp.ertBaseAddr").is_initialized()) {
        std::stringstream streamSS ;
        streamSS << std::hex << mPlatformData.get<std::string>("plp.ertBaseAddr");
        uint64_t baseAddr_u;
        streamSS >> baseAddr_u;
        return baseAddr_u;
      }
    }
    return 0;
  }

  std::shared_ptr<xrt_core::device> HwEmShim::getMCoreDevice()
  {
    return mCoreDevice;
  }

  bool HwEmShim::isLegacyErt()
  {
    if(xclemulation::config::getInstance()->getLegacyErt() == xclemulation::ertmode::legacy)
      return true;
    else if(xclemulation::config::getInstance()->getLegacyErt() == xclemulation::ertmode::updated)
      return false;

    //Following platforms uses legacyErt As per Emulation team.
    //There is no other way to get whether platform uses legacy ERT or not
    std::string vbnv  = mDeviceInfo.mName;
    if(!vbnv.empty() &&
        (  vbnv.find("u200_xdma-gen3x4_201830") != std::string::npos
        || vbnv.find("u200_xdma_201830")        != std::string::npos
        || vbnv.find("u250_qep_201910")         != std::string::npos
        || vbnv.find("u250_xdma_201830")        != std::string::npos
        || vbnv.find("u280_xdma_201920")        != std::string::npos
        || vbnv.find("u50_xdma_201910")         != std::string::npos
        || vbnv.find("u50_xdma_201920")         != std::string::npos))
      return true;

    return false;
  }

  bool HwEmShim::isCdmaEnabled()
  {
    if (xclemulation::config::getInstance()->getIsPlatformEnabled()) {
      if (mPlatformData.get_optional<std::string>("plp.numCdma").is_initialized()) {
        int numCdma = std::stoi(mPlatformData.get<std::string>("plp.numCdma"));
        return (numCdma > 0 ? true : false);
      }
    }

    return mFeatureRom.FeatureBitMap & FeatureBitMask::CDMA;
  }

  uint64_t HwEmShim::getCdmaBaseAddress(unsigned int index)
  {
    if (xclemulation::config::getInstance()->getIsPlatformEnabled()) {
      std::string cdmaAddrStr = "plp.cdmaBaseAddress" + std::to_string(index);
      if (mPlatformData.get_optional<std::string>(cdmaAddrStr).is_initialized()) {
        std::stringstream streamSS ;
        streamSS << std::hex << mPlatformData.get<std::string>(cdmaAddrStr);
        uint64_t baseAddr_u;
        streamSS >> baseAddr_u;
        return baseAddr_u;
      }
    }

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

  size_t HwEmShim::xclGetDeviceTimestamp()
  {
    bool ack = true;
    size_t deviceTimeStamp = 0;
    xclGetDeviceTimestamp_RPC_CALL(xclGetDeviceTimestamp,ack,deviceTimeStamp);
    return deviceTimeStamp;
  }

  void HwEmShim::xclReadBusStatus(xdp::MonitorType type) {

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
      systemUtil::makeSystemCall(sdxProfileKernelFile, systemUtil::systemOperation::REMOVE, "", std::to_string(__LINE__));
      std::string sdxTraceKernelFile = std::string(path) + "/timeline_kernels.csv";
      systemUtil::makeSystemCall(sdxTraceKernelFile, systemUtil::systemOperation::REMOVE, "", std::to_string(__LINE__));
    }

    std::string logFilePath = xrt_core::config::get_hal_logging();
    if (!logFilePath.empty()) {
      mLogStream.open(logFilePath);
      mLogStream << "FUNCTION, THREAD ID, ARG..." << std::endl;
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

    if (xclemulation::config::getInstance()->isMemLogsEnabled())
    {
      mGlobalInMemStream.open("global_in.mem");
      mGlobalOutMemStream.open("global_out.mem");
    }

    // Shim object creation doesn't follow xclOpen/xclClose.
    // The core device must correspond to open and close, so
    // create here rather than in constructor
    mCoreDevice = xrt_core::hwemu::get_userpf_device(this, mDeviceIndex);

    device_handles::add(this);
  }

/**********************************************HAL2 API's START HERE **********************************************/

/*********************************** Utility ******************************************/

static bool check_bo_user_flags(HwEmShim* dev, unsigned flags)
{
	const unsigned ddr_count = dev->xocl_ddr_channel_count();
	unsigned ddr;

	if(ddr_count == 0)
		return false;

	if (flags == XOCL_MEM_BANK_MSK)
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

  auto xobj = std::make_unique<xclemulation::drm_xocl_bo>();
  xobj->flags=info->flags;
  /* check whether buffer is p2p or not*/
  bool noHostMemory = xclemulation::no_host_memory(xobj.get());
  std::string sFileName("");

  if(xobj->flags & XCL_BO_FLAGS_EXECBUF)
  {
    uint64_t result = mDataSpace->alloc(size,1);
    xobj->base = result;
  }
  else
  {
    xobj->base = xclAllocDeviceBuffer2(size, XCL_MEM_DEVICE_RAM, ddr, noHostMemory, info->flags, sFileName, xobj->chunks);
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
  mXoclObjMap[mBufferCount++] = xobj.release();
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
  mFdToFileNameMap [fd] = std::make_tuple(sFileName,size,(void*)data,bo->flags);
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
    int size = std::get<1>((*itr).second);
    unsigned boFlags = std::get<3>((*itr).second);

    unsigned int importedBo = xclAllocBO(size, 0, boFlags);
    xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(importedBo);
    if(!bo)
    {
      std::cout<<"ERROR HERE in importBO "<<std::endl;
      return -1;
    }
    mImportedBOs.insert(importedBo);
    bo->fd = boGlobalHandle;
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
      << ", "<< src_boHandle << ", "<< size << ", " << dst_offset << ", " << src_offset<< std::endl;
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

  // Copy buffer thru the M2M.
    if ((deviceQuery(key_type::m2m) && getM2MAddress() != 0) && !((sBO->fd >= 0) || (dBO->fd >= 0))) {

      char hostBuf[M2M_KERNEL_ARGS_SIZE];
      std::memset(hostBuf, 0, M2M_KERNEL_ARGS_SIZE);

      //src and dest addresses construction
      uint64_t src_addr = sBO->base + src_offset;
      uint64_t dest_addr = dBO->base + dst_offset;

      //fill the hostbuf with the src offset and dest offset and size offset
      std::memcpy(hostBuf + 0x10, (unsigned char*)&src_addr, 8); //copying the src address to the hostbuf to the specified offset by M2M IP
      std::memcpy(hostBuf + 0x18, (unsigned char*)&dest_addr, 8);  //copying the dest address to the hostbuf to the specified offset by M2M IP
      std::memcpy(hostBuf + 0x20, (unsigned char*)&size, 4); //copying the size address to the hostbuf to the specified offset by M2M IP

      //Configuring the kernel with hostbuf by providing the Base address of the IP
      if (xclWrite(XCL_ADDR_KERNEL_CTRL, getErtBaseAddress() + 0x20000, hostBuf, M2M_KERNEL_ARGS_SIZE) != M2M_KERNEL_ARGS_SIZE) {
        std::cerr << "ERROR: Failed to write to args to the m2m IP" << std::endl;
      }

      hostBuf[0] = 0x1; //filling the hostbuf with the start info
      //Starting the kernel
      if (xclWrite(XCL_ADDR_KERNEL_CTRL, getErtBaseAddress() + 0x20000, hostBuf, 4) != 4) {
        std::cerr << "ERROR: Failed to start the m2m kernel" << std::endl;
      }

      do {
        //Read the status of the kernel by polling the hostBuf[0]
        //check for the base_address is either 4 or 6
        xclRead(XCL_ADDR_KERNEL_CTRL, getErtBaseAddress() + 0x20000, hostBuf, 4);
      } while (!(hostBuf[0] & (CONTROL_AP_DONE | CONTROL_AP_IDLE)));

      PRINTENDFUNC;
      return 0;
    }

  // source buffer is host_only and destination buffer is device_only
  if (isHostOnlyBuffer(sBO) && !xclemulation::xocl_bo_p2p(sBO) && xclemulation::xocl_bo_dev_only(dBO)) {
    unsigned char* host_only_buffer = (unsigned char*)(sBO->buf) + src_offset;
    if (xclCopyBufferHost2Device(dBO->base, (void*)host_only_buffer, size, dst_offset, dBO->topology) != size) {
      return -1;
    }
  } // source buffer is device_only and destination buffer is host_only
  else if (isHostOnlyBuffer(dBO) && !xclemulation::xocl_bo_p2p(dBO) && xclemulation::xocl_bo_dev_only(sBO)) {
    unsigned char* host_only_buffer = (unsigned char*)(dBO->buf) + dst_offset;
    if (xclCopyBufferDevice2Host((void*)host_only_buffer, sBO->base, size, src_offset, sBO->topology) != size) {
      return -1;
    }
  }// source and destination buffers are device_only
  else if (!isHostOnlyBuffer(sBO) && !isHostOnlyBuffer(dBO) && (dBO->fd < 0) && (sBO->fd < 0)) {
    unsigned char temp_buffer[size];
    // copy data from source buffer to temp buffer
    if (xclCopyBufferDevice2Host((void*)temp_buffer, sBO->base, size, src_offset, sBO->topology) != size) {
      std::cerr << "ERROR: copy buffer from device to host failed " << std::endl;
      return -1;
    }
    // copy data from temp buffer to destination buffer
    if (xclCopyBufferHost2Device(dBO->base, (void*)temp_buffer, size, dst_offset, dBO->topology) != size) {
      std::cerr << "ERROR: copy buffer from host to device failed " << std::endl;
      return -1;
    }
  }
  else if((sBO->fd >=0) && (dBO->fd >= 0)) {  //Both source & destination P2P buffer
    // CR-1113695 Copy data from source P2P to Dest P2P
    std::vector<char> temp_buffer(size);
    lseek(sBO->fd, src_offset, SEEK_SET);
    int bytes_read = read(sBO->fd, temp_buffer.data(), size);
    if (bytes_read) {
      if (mLogStream.is_open())
      {
        mLogStream << __func__ << ", data read successfully from the src fd to local buffer." << std::endl;
      }
    }

    lseek(dBO->fd, dst_offset, SEEK_SET);
    int bytes_write = write(dBO->fd, temp_buffer.data(), size);
    if (bytes_write) {
      if (mLogStream.is_open())
      {
        mLogStream << __func__ << ", data written successfully from local buffer to dest fd." << std::endl;
      }
    }
  }
  else if(dBO->fd >= 0){  //destination p2p buffer
    // CR-1113695 Copy data from temp buffer to exported fd
    std::vector<char> temp_buffer(size);
    if (xclCopyBufferDevice2Host((void*)temp_buffer.data(), sBO->base, size, src_offset, sBO->topology) != size) {
      std::cerr << "ERROR: copy buffer from device to host failed " << std::endl;
      return -1;
    }
    lseek(dBO->fd, dst_offset, SEEK_SET);
    int bytes_write = write(dBO->fd, temp_buffer.data(), size);

    if (bytes_write) {
      if (mLogStream.is_open())
      {
        mLogStream << __func__ << ", data written successfully from local buffer to dest fd." << std::endl;
      }
    }
  }
  else if (sBO->fd >= 0) {  //source p2p buffer
    // CR-1112934 Copy data from exported fd to temp buffer using read API
    std::vector<char> temp_buffer(size);
    lseek(sBO->fd, src_offset, SEEK_SET);
    int bytes_read = read(sBO->fd, temp_buffer.data(), size);

    if (bytes_read) {
      if (mLogStream.is_open())
      {
        mLogStream << __func__ << ", data read successfully from the src fd to local buffer." << std::endl;
      }
    }

    // copy data from temp buffer to destination buffer
    if (xclCopyBufferHost2Device(dBO->base, (void*)temp_buffer.data(), size, dst_offset, dBO->topology) != size) {
      std::cerr << "ERROR: copy buffer from host to device failed " << std::endl;
      return -1;
    }
  }
  else{
     std::cerr << "ERROR: Copy buffer from source to destination failed" << std::endl;
     return -1;
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
  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(boHandle);
  if (!bo) {
    PRINTENDFUNC;
    return nullptr;
  }

  std::string sFileName = bo->filename;
  if(!sFileName.empty() ) // In case of peer-to-peer
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
    mFdToFileNameMap [fd] = std::make_tuple(sFileName,bo->size,(void*)data, bo->flags);
    bo->buf = data;
    PRINTENDFUNC;
    return data;
  }

  void *pBuf=nullptr;
  if (posix_memalign(&pBuf, getpagesize(), bo->size))
  {
    if (mLogStream.is_open()) mLogStream << "posix_memalign failed" << std::endl;
    pBuf=nullptr;
    return pBuf;
  }
  memset(pBuf, 0, bo->size);
  bo->buf = pBuf;

  //For Slave Bridge scenario, maintaining the map for base vs pBuf pointer
  if (isHostOnlyBuffer(bo)) {
    mHostOnlyMemMap[bo->base] = std::make_pair(pBuf, bo->size);
  }

  PRINTENDFUNC;
  return pBuf;
}

int HwEmShim::xclUnmapBO(unsigned int boHandle, void* addr)
{
  std::lock_guard<std::mutex> lk(mApiMtx);
  auto bo = xclGetBoByHandle(boHandle);
  return bo ? munmap(addr, bo->size) : -1;
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
  if (!isHostOnlyBuffer(bo)) { // bypassed the xclCopyBufferDevice2Host/Host2Device RPC calls for Slave Bridge (host only buffer scenario)
    void* buffer = bo->userptr ? bo->userptr : bo->buf;
    if (dir == XCL_BO_SYNC_BO_TO_DEVICE)
    {
      if (xclCopyBufferHost2Device(bo->base, buffer, size, offset, bo->topology) != size)
      {
        returnVal = EIO;
      }
    }
    else
    {
      if (xclCopyBufferDevice2Host(buffer, bo->base, size, offset, bo->topology) != size)
      {
        returnVal = EIO;
      }
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
    bool bSendToSim = true;
    if(bo->flags & XCL_BO_FLAGS_EXECBUF)
      bSendToSim = false;

    if(bo->chunks.size())
    {
	    for(auto it: bo->chunks)
		    xclFreeDeviceBuffer(it.first,bSendToSim);

    }
    else
    {
	    xclFreeDeviceBuffer(bo->base, bSendToSim);
    }
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
  int ret=-1;
  if(xclemulation::config::getInstance()->isNewMbscheduler())
  {
      if(!m_scheduler || !bo)
      {
          PRINTENDFUNC;
          return ret;
      }
      ret = m_scheduler->add_exec_buffer(bo);
      PRINTENDFUNC;
      return ret;
  } else if (xclemulation::config::getInstance()->isXgqMode()) {
      if (!m_xgq || !bo)
      {
          PRINTENDFUNC;
          return ret;
      }
      ret = m_xgq->add_exec_buffer(bo);
      PRINTENDFUNC;
      return ret;
  } else {
    if(!mMBSch || !bo)
    {
      PRINTENDFUNC;
      return ret;
    }
    ret = mMBSch->add_exec_buffer(mCore, bo);
    PRINTENDFUNC;
  }
  return ret;
}

/*
 * xclExecBuf()
 */
int HwEmShim::xclExecBuf(unsigned int cmdBO, size_t num_bo_in_wait_list, unsigned int *bo_wait_list)
{
  if (mLogStream.is_open())
  {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << cmdBO << ", " << num_bo_in_wait_list << ", " << bo_wait_list << std::endl;
  }

  xclemulation::drm_xocl_bo* bo = xclGetBoByHandle(cmdBO);

  xcl_LogMsg(XRT_INFO, "", "%s, cmdBO: %d, num_bo_in_wait_list: %d, bo_wait_list: %d",
            __func__, cmdBO, num_bo_in_wait_list, bo_wait_list);

  if (num_bo_in_wait_list > MAX_DEPS) {
    xcl_LogMsg(XRT_ERROR, "", "%s, Incorrect argument. Max num of BOs in wait_list: %d",
        __func__, MAX_DEPS);

    return -EINVAL;
  }

  /*int ret;
  unsigned int bwl[8] = {0};
  std::memcpy(bwl,bo_wait_list,num_bo_in_wait_list*sizeof(unsigned int));
  drm_xocl_execbuf exec = {0, cmdBO, bwl[0],bwl[1],bwl[2],bwl[3],bwl[4],bwl[5],bwl[6],bwl[7]};
  ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_EXECBUF, &exec);
  return ret ? -errno : ret;*/

  int ret=-1;
  if(xclemulation::config::getInstance()->isNewMbscheduler())
  {
      if(!m_scheduler || !bo)
      {
          PRINTENDFUNC;
          return ret;
      }
      ret = m_scheduler->add_exec_buffer(bo);
      PRINTENDFUNC;
  } else {
    ret = mMBSch->add_exec_buffer(mCore, bo);
    PRINTENDFUNC;
  }
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

////////////////////////////////////////////////////////////////
// Context handling
////////////////////////////////////////////////////////////////
int
HwEmShim::
xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  // When properly implemented this function must throw on error
  // and any exception must be caught by global xclOpenContext and
  // converted to error code
  return 0;
}

int
HwEmShim::
xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex)
{
  return 0;
}

// aka xclOpenContextByName, internal shim API for native C++ applications only
// Once properly implemented, this API should throw on error
xrt_core::cuidx_type
HwEmShim::
open_cu_context(const xrt::hw_context& hwctx, const std::string& cuname)
{
  // Emulation does not yet support multiple xclbins.  Call
  // regular flow.  Default access mode to shared unless explicitly
  // exclusive.
  auto shared = (hwctx.get_mode() != xrt::hw_context::access_mode::exclusive);
  auto ctxhdl = static_cast<xcl_hwctx_handle>(hwctx);
  auto cuidx = mCoreDevice->get_cuidx(ctxhdl, cuname);
  xclOpenContext(hwctx.get_xclbin_uuid().get(), cuidx.index, shared);

  return cuidx;
}

void
HwEmShim::
close_cu_context(const xrt::hw_context& hwctx, xrt_core::cuidx_type cuidx)
{
  if (xclCloseContext(hwctx.get_xclbin_uuid().get(), cuidx.index))
    throw xrt_core::system_error(errno, "failed to close cu context (" + std::to_string(cuidx.index) + ")");
}

// aka xclCreateHWContext, internal shim API for native C++ applications only
// Once properly implemented, this API should throw on error
uint32_t // ctx handle aka slot idx
HwEmShim::
create_hw_context(const xrt::uuid&, const xrt::hw_context::qos_type&, xrt::hw_context::access_mode)
{
  // Explicit hardware contexts are not yet supported
  throw xrt_core::ishim::not_supported_error{__func__};
}

// aka xclDestroyHWContext, internal shim API for native C++ applications only
// Once properly implemented, this API should throw on error
void
HwEmShim::
destroy_hw_context(uint32_t ctxhdl)
{
  // Explicit hardware contexts are not yet supported
  throw xrt_core::ishim::not_supported_error{__func__};
}

// aka xclRegisterXclbin, internal shim API for native C++ applications only
// Once properly implemented, this API should throw on error
void
HwEmShim::
register_xclbin(const xrt::xclbin&)
{
  // Explicit hardware contexts are not yet supported
  throw xrt_core::ishim::not_supported_error{__func__};
}
////////////////////////////////////////////////////////////////

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
  uint32_t bytesPerSample = (xdp::TRACE_FIFO_WORD_WIDTH / 8);

  traceBufSz = xdp::MAX_TRACE_NUMBER_SAMPLES_FIFO * bytesPerSample;   /* Buffer size in bytes */
  traceSamples = nSamples;

  return 0;
}

int HwEmShim::xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
    // Create trace buffer on host (requires alignment)
    const int traceBufWordSz = traceBufSz / 4;  // traceBufSz is in number of bytes

    uint32_t size = 0;

    wordsPerSample = (xdp::TRACE_FIFO_WORD_WIDTH / 32);
    uint32_t numWords = numSamples * wordsPerSample;

//    alignas is defined in c++11
#ifndef _WINDOWS
    /* Alignment is limited to 16 by PPC64LE : so , should it be
    alignas(16) uint32_t hostbuf[traceBufSzInWords];
    */
    alignas(xdp::IP::FIFO::alignment) uint32_t hostbuf[traceBufWordSz];
#else
    xrt_core::AlignedAllocator<uint32_t> alignedBuffer(xdp::IP::FIFO::alignment, traceBufWordSz);
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

double HwEmShim::xclGetDeviceClockFreqMHz()
{
  //return 1.0;
  double clockSpeed;
  //300.0 MHz
  clockSpeed = 300.0;
  return clockSpeed;
}

// For PCIe gen 3x16 or 4x8:
// Max BW = 16.0 * (128b/130b encoding) = 15.75385 GB/s
double HwEmShim::xclGetHostReadMaxBandwidthMBps()
{
  return 15753.85;
}

// For PCIe gen 3x16 or 4x8:
// Max BW = 16.0 * (128b/130b encoding) = 15.75385 GB/s
double HwEmShim::xclGetHostWriteMaxBandwidthMBps()
{
  return 15753.85;
}

// For DDR4: Typical Max BW = 19.25 GB/s
double HwEmShim::xclGetKernelReadMaxBandwidthMBps()
{
  return 19250.00;
}

// For DDR4: Typical Max BW = 19.25 GB/s
double HwEmShim::xclGetKernelWriteMaxBandwidthMBps()
{
  return 19250.00;
}

uint32_t HwEmShim::getPerfMonNumberSlots(xdp::MonitorType type)
{
  if (type == xdp::MonitorType::memory)
    return mMemoryProfilingNumberSlots;
  if (type == xdp::MonitorType::accel)
    return mAccelProfilingNumberSlots;
  if (type == xdp::MonitorType::stall)
    return mStallProfilingNumberSlots;
  if (type == xdp::MonitorType::host)
    return 1;
  if (type == xdp::MonitorType::str)
    return mStreamProfilingNumberSlots;

  return 0;
}

// Get slot name
void HwEmShim::getPerfMonSlotName(xdp::MonitorType type, uint32_t slotnum,
                                  char* slotName, uint32_t length) {
  std::string str = "";
  if (type == xdp::MonitorType::memory) {
    str = (slotnum < xdp::MAX_NUM_AIMS) ? mPerfMonSlotName[slotnum] : "";
  }
  if (type == xdp::MonitorType::accel) {
    str = (slotnum < xdp::MAX_NUM_AMS) ? mAccelMonSlotName[slotnum] : "";
  }
  if (type == xdp::MonitorType::str) {
    str = (slotnum < xdp::MAX_NUM_ASMS) ? mStreamMonSlotName[slotnum] : "";
  }
  if(str.length() < length) {
   strncpy(slotName, str.c_str(), length);
  } else {
   strncpy(slotName, str.c_str(), length-1);
   slotName[length-1] = '\0';
  }
}

int HwEmShim::xcl_LogMsg(xrtLogMsgLevel level, const char* tag, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  int ret = xclLogMsg(level, tag, format, args);
  va_end(args);
  return ret;
}

/*
 * xclLogMsg()
 */
int HwEmShim::xclLogMsg(xrtLogMsgLevel level, const char* tag, const char* format, va_list args1)
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

void HwEmShim::closeMessengerThread()
{
  // set_simulator_started has to be false in order to see proper exit of joinable thread.
  //set_simulator_started(false);
  if (mMessengerThread.joinable()) {
    mMessengerThreadStarted = false;
    mMessengerThread.join();
  }

  if (mHostMemAccessThread.joinable()) {
    mHostMemAccessThreadStarted = false;
    mHostMemAccessThread.join();
  }
}

//CU register space for xclRegRead/Write()
int HwEmShim::xclRegRW(bool rd, uint32_t cu_index, uint32_t offset, uint32_t *datap)
{
  if (mLogStream.is_open()) {
    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << "CU Idx : " << cu_index << " Offset : " << offset << " Datap : " << (*datap) << std::endl;
  }

  // get sorted cu addresses to match up with cu_index
  const auto& cuidx2addr = mCoreDevice->get_cus();
  if (cu_index >= cuidx2addr.size()) {
    std::string strMsg = "ERROR: [HW-EMU 20] xclRegRW - invalid CU index: " + std::to_string(cu_index);
    logMessage(strMsg);
    return -EINVAL;
  }

  uint64_t cuAddRange = 64 * 1024;
  for (auto cuInfo : mCURangeMap) {
    std::string instName = cuInfo.first;
    int cuIdx = static_cast<int>(cu_index);
    int tmpCuIdx = xclIPName2Index(instName.c_str());

    if (tmpCuIdx == cuIdx) {
      cuAddRange = cuInfo.second;
    }
  }

  if (offset >= cuAddRange || (offset & (sizeof(uint32_t) - 1)) != 0) {
    std::string strMsg = "ERROR: [HW-EMU 21] xclRegRW - invalid CU offset: " + std::to_string(offset);
    logMessage(strMsg);
    return -EINVAL;
  }

  std::array<char, REG_BUFF_SIZE> buff;

  uint64_t baseAddr = cuidx2addr[cu_index] + offset;
  if (rd) {
    if (xclRead(XCL_ADDR_KERNEL_CTRL, baseAddr, buff.data(), REG_BUFF_SIZE) != REG_BUFF_SIZE) {
      std::string strMsg = "ERROR: [HW-EMU 22] xclRegRW - xclRead failed for CU: " + std::to_string(cu_index);
      logMessage(strMsg);
      return -EINVAL;
    }
    auto tmp_buff = reinterpret_cast<uint32_t*>(buff.data());
    *datap = tmp_buff[0];
  }
  else {
    uint32_t * tmp_buff = reinterpret_cast<uint32_t*>(buff.data());
    tmp_buff[0] = *datap;
    if (xclWrite(XCL_ADDR_KERNEL_CTRL, baseAddr, tmp_buff, REG_BUFF_SIZE) != REG_BUFF_SIZE) {
      std::string strMsg = "ERROR: [HW-EMU 23] xclRegRW - xclWrite failed for CU: " + std::to_string(cu_index);
      logMessage(strMsg);
      return -EINVAL;
    }
  }
  return 0;
}

int HwEmShim::xclRegRead(uint32_t cu_index, uint32_t offset, uint32_t *datap)
{
  return xclRegRW(true, cu_index, offset, datap);
}

int HwEmShim::xclRegWrite(uint32_t cu_index, uint32_t offset, uint32_t data)
{
  return xclRegRW(false, cu_index, offset, &data);
}

//Get CU index from IP_LAYOUT section for corresponding kernel name
int HwEmShim::xclIPName2Index(const char *name)
{
  //Get IP_LAYOUT buffer from xclbin
  auto buffer = mCoreDevice->get_axlf_section(IP_LAYOUT);
  return xclemulation::getIPName2Index(name, buffer.first);
}


/********************************************** QDMA APIs IMPLEMENTATION END**********************************************/
/**********************************************HAL2 API's END HERE **********************************************/

/********************************************** Q2H_helper class implementation starts **********************************************/
/**
 * Function: device2xrt_rd_trans_cb
 * Description : Its a Read request from Device to HOST Buffer Call back function which gets read Address,
 *               size and Data pointer to be filled.
 * Arguments:
 *   1. addr: Read request addr
 *   2. data_ptr: container of read data which gets filled by Host. size of this container
 *                is = size rgument of this function
 *   3. size: size of read request
 * Return Value: This funtion returns boolean value. Incase of successful read from Host
 *                  it will return true or else false.
 *
 **/
bool HwEmShim::device2xrt_rd_trans_cb(unsigned long int addr, void* const data_ptr,unsigned long int size) {

  auto itStart = mHostOnlyMemMap.begin();
  auto itEnd = mHostOnlyMemMap.end();
  while (itStart != itEnd)
  {
    uint64_t baseAddress = (*itStart).first;
    std::pair<void*, uint64_t> osAddressSizePair = (*itStart).second;
    void* startOSAddress = osAddressSizePair.first;
    uint64_t buf_size = osAddressSizePair.second;

    if (addr >= baseAddress && addr < baseAddress + buf_size)
    {
      unsigned char* finalOsAddress = (unsigned char*)startOSAddress + (addr - baseAddress);

      if ((addr + size) > (baseAddress + buf_size)) {
        std::string dMsg = "ERROR: [HW-EMU 24] Host Memory - Accessing the invalid address range which is not within the boundary. Valid address range is "
          + std::to_string(baseAddress) + " - " + std::to_string(baseAddress + buf_size) + ". Whereas requested address range is " + std::to_string(addr) + " - " + std::to_string(addr+size);
        logMessage(dMsg, 0);

        return false;
      }

      std::memcpy((unsigned char*)data_ptr, finalOsAddress, size);
      break;
    }

    itStart++;
  }

  return true;
}

/**
 * Function: device2xrt_wr_trans_cb
 * Description : Its a Write request from Device to HOST Buffer Call back function which gets Write address,
 *               size and Data pointer of written data.
 * Arguments:
 *   1. addr: Read request addr
 *   2. data_ptr: container which holds write data which is already filled by deivce. size of this container
 *                is = size rgument of this function
 *   3. size: size of Write request
 * Return Value: This funtion returns boolean value. Incase of successful write to Host Buffer
 *                  it will return true or else false.
 *
 **/
bool HwEmShim::device2xrt_wr_trans_cb(unsigned long int addr, void const* data_ptr,unsigned long int size) {

  auto itStart = mHostOnlyMemMap.begin();
  auto itEnd = mHostOnlyMemMap.end();
  while (itStart != itEnd)
  {
    uint64_t baseAddress = (*itStart).first;
    std::pair<void*, uint64_t> osAddressSizePair = (*itStart).second;
    void* startOSAddress = osAddressSizePair.first;
    uint64_t buf_size = osAddressSizePair.second;

    if (addr >= baseAddress && addr < baseAddress + buf_size)
    {
      unsigned char* finalOsAddress = (unsigned char*)startOSAddress + (addr - baseAddress);

      if ((addr + size) > (baseAddress + buf_size)) {
        std::string dMsg = "ERROR: [HW-EMU 25] Host Memory - Accessing the invalid address range which is not within the boundary. Valid address range is "
          + std::to_string(baseAddress) + " - " + std::to_string(baseAddress + buf_size) + ". Whereas requested address range is " + std::to_string(addr) + " - " + std::to_string(addr + size);
        logMessage(dMsg, 0);

        return false;
      }

      std::memcpy(finalOsAddress, (unsigned char*)data_ptr, size);
      break;
    }

    itStart++;
  }

  return true;
}
bool HwEmShim::device2xrt_irq_trans_cb(uint32_t,unsigned long int) {
    // TODO: We need to return a ERROR here as we are not supporting this. Its helps users to get notified
    return true;
}
Q2H_helper :: Q2H_helper(xclhwemhal2::HwEmShim* _inst) {
    header          = std::make_unique<call_packet_info>();
    response_header = std::make_unique<response_packet_info>();
    inst = _inst;
    Q2h_sock        = nullptr;
    header->set_size(0);
    header->set_xcl_api(0);
    response_header->set_size(0);
    response_header->set_xcl_api(0);
#if GOOGLE_PROTOBUF_VERSION < 3006001
    i_len           = header->ByteSize();
    ri_len          = response_header->ByteSize();
#else
    i_len           = header->ByteSizeLong();
    ri_len          = response_header->ByteSizeLong();
#endif
}
Q2H_helper::~Q2H_helper() = default;

/**
 * Pooling on socket for any memory or interrupt requests from SIM_QDMA
 *
 */
int Q2H_helper::poolingon_Qdma() {
    //Getting incoming header packet from sim_qdma
    auto raw_header = std::make_unique<char[]>(i_len);
    int r = Q2h_sock->sk_read((void*)raw_header.get(), i_len);
    if (r <= 0) {
    	return r;
    }

    assert(i_len == (uint32_t)r);
    //deserializing protobuf message
    header->ParseFromArray((void*)raw_header.get(), i_len);
    if (header->xcl_api() == xclClose_n) {
        return -1;
    }
    //Getting incoming header packet from sim_qdma
    auto raw_payload = std::make_unique<char[]>(header->size());
    r = Q2h_sock->sk_read((void*)raw_payload.get(), header->size());
    assert((uint32_t)r == header->size());
    //processing payload and sending the response message back to sim_qdma
    if (header->xcl_api() == xclQdma2HostReadMem_n) {
    	xclSlaveReadReq_call payload;
        xclSlaveReadReq_response response_payload;
    	payload.ParseFromArray((void*)raw_payload.get(), r);
        auto data = std::make_unique<char[]>(payload.size());
        bool resp = inst->device2xrt_rd_trans_cb((unsigned long int)payload.addr(),(void* const)data.get(),(unsigned long int)payload.size());
        response_payload.set_valid(resp);
        response_payload.set_data((void*)data.get(),payload.size());
#if GOOGLE_PROTOBUF_VERSION < 3006001
        auto r_len = response_payload.ByteSize();
#else
        auto r_len = response_payload.ByteSizeLong();
#endif
        SEND_RESP2QDMA()
    }
    if (header->xcl_api() == xclQdma2HostWriteMem_n) {
    	xclSlaveWriteReq_call payload;
        xclSlaveWriteReq_response response_payload;
    	payload.ParseFromArray((void*)raw_payload.get(), r);
        bool resp = inst->device2xrt_wr_trans_cb((unsigned long int)payload.addr(),(void const*)payload.data().c_str(),(unsigned long int)payload.size());
        response_payload.set_valid(resp);
#if GOOGLE_PROTOBUF_VERSION < 3006001
        auto r_len = response_payload.ByteSize();
#else
        auto r_len = response_payload.ByteSizeLong();
#endif
        SEND_RESP2QDMA()
    }
    if (header->xcl_api() == xclQdma2HostInterrupt_n) {
    	xclInterruptOccured_call payload;
        xclInterruptOccured_response response_payload;
    	payload.ParseFromArray((void*)raw_payload.get(), r);
        uint32_t interrupt_line = payload.interrupt_line();
        bool resp = inst->device2xrt_irq_trans_cb(interrupt_line,4);
        response_payload.set_valid(resp);
#if GOOGLE_PROTOBUF_VERSION < 3006001
        auto r_len = response_payload.ByteSize();
#else
        auto r_len = response_payload.ByteSizeLong();
#endif
        SEND_RESP2QDMA()
    }

    return 1;
}

bool Q2H_helper::connect_sock() {
    std::string sock_name;
    if (getenv("EMULATION_SOCKETID")) {
      sock_name = "D2X_unix_sock_" + std::string(getenv("EMULATION_SOCKETID"));
    } else {
      sock_name = "D2X_unix_sock";
    }
    if (Q2h_sock == nullptr) {
      Q2h_sock = std::make_unique<unix_socket>("EMULATION_SOCKETID", sock_name, 5, false);
    }
    else if (!Q2h_sock->server_started) {
      Q2h_sock->start_server(5,false);
    }
    return Q2h_sock->server_started;
}

void HwEmShim::hostMemAccessThread() {
    mHostMemAccessThreadStarted.store(true);
    auto mq2h_helper_ptr = std::make_unique<Q2H_helper>(this);
    bool sock_ret = false;
    int count = 0;
    while (mHostMemAccessThreadStarted && !sock_ret && count < 71) {
        sock_ret = mq2h_helper_ptr->connect_sock();
        count++;
    }
    if (not sock_ret) {
      std::cout<<"\n unable to get a reliable socket connection, ideally should exit here. select call took care. \n";
    }
    int r =0;
    while (mHostMemAccessThreadStarted && r >= 0) {
        try {
            if (not get_simulator_started())
                return;
            r = mq2h_helper_ptr->poolingon_Qdma();
        } catch(int e) {
            std::cout << " Exception during socket communitication between SIM_QDMA ---> HE_EMU driver.." << std::endl;
        }
    }
}
/********************************************** Q2H_helper class implementation Ends **********************************************/
}  // end namespace xclhwemhal2
