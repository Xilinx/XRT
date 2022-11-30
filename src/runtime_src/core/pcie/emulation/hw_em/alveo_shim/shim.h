// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _HW_EM_SHIM_H_
#define _HW_EM_SHIM_H_

#ifndef _WINDOWS
#include "config.h"
#include "em_defines.h"

#include "core/common/api/xclbin_int.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/query_requests.h"
#include "core/common/scheduler.h"
#include "core/common/xrt_profiling.h"
#include "core/include/experimental/xrt_hw_context.h"
#include "core/include/experimental/xrt_xclbin.h"
#include "core/include/xdp/common.h"
#include "core/include/xdp/counters.h"

#include "mbscheduler.h"
#include "memorymanager.h"
#include "mbscheduler_hwemu.h"
#include "mem_model.h"
#include "rpc_messages.pb.h"
#include "xgq_hwemu.h"
#include "xclbin.h"
#include "xcl_api_macros.h"
#include "xcl_macros.h"
#include "unix_socket.h"
#include "nocddr_fastaccess_hwemu.h"

#endif

#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cstdarg>
#include <thread>
#include <tuple>

#ifdef _WINDOWS
#define strtoll _strtoi64
#endif

namespace xclhwemhal2 {
using key_type = xrt_core::query::key_type;
using addr_type = uint64_t;
#define PRINTENDFUNC if (mLogStream.is_open()) mLogStream << __func__ << " ended " << std::endl;

  class Event {
    public:
      uint8_t awlen;
      uint8_t arlen;
      uint8_t eventflags;
      uint32_t timestamp;
      uint64_t host_timestamp;
      uint16_t readBytes;
      uint16_t writeBytes;
      Event();
  };

  struct membank
  {
    addr_type base_addr; // base address of bank
    std::string tag;     // bank tag in lowercase
    uint64_t size;       // size of this bank in bytes
    int32_t index;       // bank id
  };

  typedef struct loadBitStream {
    char* m_zipFile;
    size_t m_zipFileSize;
    char* m_xmlfile;
    size_t m_xmlFileSize;
    char* m_debugFile;
    size_t m_debugFileSize;
    char* m_memTopology;
    size_t m_memTopologySize;
    char* m_pdi;
    size_t m_pdiSize;
    char* m_emuData;
    size_t m_emuDataSize;
    const axlf* m_top;
  } bitStreamArg;

 typedef struct
 {
   std::string name;
   unsigned int size;
 } KernelArg;


  class HwEmShim {

    public:

      // HAL2 RELATED member functions start
      unsigned int xclAllocBO(size_t size, int unused, unsigned flags);
      uint64_t xoclCreateBo(xclemulation::xocl_create_bo *info);
      void* xclMapBO(unsigned int boHandle, bool write);
      int xclUnmapBO(unsigned int boHandle, void* addr);
      int xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset);
      unsigned int xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags);
      int xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties);
      size_t xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek);
      size_t xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
      void xclFreeBO(unsigned int boHandle);
      ssize_t xclUnmgdPwrite(unsigned flags, const void *buf, size_t count, uint64_t offset);
      ssize_t xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset);

      static int xcl_LogMsg(xrtLogMsgLevel level, const char* tag, const char* format, ...);
      static int xclLogMsg(xrtLogMsgLevel level, const char* tag, const char* format, va_list args1);

      // P2P Support
      int xclExportBO(unsigned int boHandle);
      unsigned int xclImportBO(int boGlobalHandle, unsigned flags);
      int xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset);

      // MB scheduler related API's
      int xclExecBuf( unsigned int cmdBO);
      int xclExecBuf(unsigned int cmdBO, size_t num_bo_in_wait_list, unsigned int *bo_wait_list);

      ////////////////////////////////////////////////////////////////
      // Context handling
      ////////////////////////////////////////////////////////////////
      int
      xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared);

      int
      xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex);

      // aka xclOpenContextByName, internal shim API for native C++ applications only
      xrt_core::cuidx_type
      open_cu_context(const xrt::hw_context& hwctx, const std::string& cuname);

      void
      close_cu_context(const xrt::hw_context& hwctx, xrt_core::cuidx_type cuidx);

      // aka xclCreateHWContext, internal shim API for native C++ applications only
      uint32_t // ctx handle aka slot idx
      create_hw_context(const xrt::uuid&, const xrt::hw_context::qos_type&, xrt::hw_context::access_mode);

      // aka xclDestroyHWContext, internal shim API for native C++ applications only
      void
      destroy_hw_context(uint32_t ctxhdl);

      // aka xclRegisterXclbin, internal shim API for native C++ applications only
      void
      register_xclbin(const xrt::xclbin&);
      ////////////////////////////////////////////////////////////////

      int xclRegisterEventNotify( unsigned int userInterrupt, int fd);
      int xclExecWait( int timeoutMilliSec);
      struct exec_core* getExecCore() { return mCore; }
      MBScheduler* getScheduler() { return mMBSch; }

      xclemulation::drm_xocl_bo* xclGetBoByHandle(unsigned int boHandle);
      inline unsigned short xocl_ddr_channel_count();
      inline unsigned long long xocl_ddr_channel_size();
      // HAL2 RELATED member functions end

      // Bitstreams
      int xclLoadXclBin(const xclBin *buffer);
      //int xclLoadBitstream(const char *fileName);
      int xclLoadBitstreamWorker(bitStreamArg);
      bool isUltraScale() const;
      int xclUpgradeFirmware(const char *fileName);
      int xclBootFPGA();
      int resetProgram(bool saveWdb=true);
      int xclGetDeviceInfo2(xclDeviceInfo2 *info);

      // Raw read/write
      size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
      size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
      size_t xclReadModifyWrite(uint64_t offset, const void *hostBuf, size_t size);
      size_t xclReadSkipCopy(uint64_t offset, void *hostBuf, size_t size);

      // Buffer management
      uint64_t xclAllocDeviceBuffer(size_t size);
      uint64_t xclAllocDeviceBuffer2(size_t& size, xclMemoryDomains domain, unsigned flags, bool p2pBuffer, unsigned boFlags, std::string &sFileName,std::map<uint64_t,uint64_t>& chunks);

      void xclOpen(const char* logfileName);
      void xclFreeDeviceBuffer(uint64_t buf,bool sendtosim);
      size_t xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek, uint32_t topology);
      size_t xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip, uint32_t topology);
      void xclClose(bool DonotRunParseLog = false);
      unsigned int xclProbe();

      //Performance Monitor APIs
      double xclGetDeviceClockFreqMHz();
      double xclGetHostReadMaxBandwidthMBps();
      double xclGetHostWriteMaxBandwidthMBps();
      double xclGetKernelReadMaxBandwidthMBps();
      double xclGetKernelWriteMaxBandwidthMBps();
      size_t xclGetDeviceTimestamp();
      void xclReadBusStatus(xdp::MonitorType type);
      void xclGetDebugMessages(bool force = false);
      void logMessage(std::string& msg,int verbosity = 0);

      // debug/profiling helpers
      void readDebugIpLayout(const std::string debugFileName);
      uint32_t getIPCountAddrNames(const std::string debugFileName, int type, uint64_t *baseAddress,
                                   std::string * portNames, uint8_t *properties, size_t size);
      void getPerfMonSlotName(xdp::MonitorType type, uint32_t slotnum, char* slotName, uint32_t length);
      uint32_t getPerfMonNumberSlots(xdp::MonitorType type);

      int xclGetDebugIPlayoutPath(char* layoutPath, size_t size);
      int xclGetTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz);
      int xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample);

      //Utility Function
      void set_simulator_started(bool val){ simulator_started = val;}
      bool get_simulator_started() {return simulator_started;}
      void fillDeviceInfo(xclDeviceInfo2* dest, xclDeviceInfo2* src);
      void saveWaveDataBase();
      void extractEmuData(const std::string& simPath, int binaryCounter, bitStreamArg args);
      void nocMmapInitialization(const std::string &simPath);
      void getDtbs(const std::string& emu_data_path, std::string& qemu_dtb, std::string& pmc_dtb);

      // Sanity checks
      static HwEmShim *handleCheck(void *handle);
      uint32_t getAddressSpace (uint32_t topology);

      //constructor
      HwEmShim( unsigned int deviceIndex, xclDeviceInfo2 &info, std::list<xclemulation::DDRBank>& DDRBankList, bool bUnified,
        bool bXPR, FeatureRomHeader &featureRom, const boost::property_tree::ptree& platformData);

      //destructor
      ~HwEmShim();

      static const int SPIR_ADDRSPACE_PRIVATE;  //0
      static const int SPIR_ADDRSPACE_GLOBAL;   //1
      static const int SPIR_ADDRSPACE_CONSTANT; //2
      static const int SPIR_ADDRSPACE_LOCAL;    //3
      static const int SPIR_ADDRSPACE_PIPES;    //4

      static const unsigned CONTROL_AP_START;
      static const unsigned CONTROL_AP_DONE;
      static const unsigned CONTROL_AP_IDLE;
      static const unsigned CONTROL_AP_CONTINUE;
      static const unsigned REG_BUFF_SIZE;
      static const unsigned M2M_KERNEL_ARGS_SIZE;

      bool isUnified()               { return bUnified; }
      void setUnified(bool _unified) { bUnified = _unified; }

      bool isMBSchedulerEnabled();
      uint64_t getM2MAddress();
      uint64_t getErtCmdQAddress();
      uint64_t getErtBaseAddress();
      int deviceQuery(key_type queryKey);

      std::string getERTVersion();
      bool isLegacyErt();
      unsigned int getDsaVersion();
      bool isCdmaEnabled();
      uint64_t getCdmaBaseAddress(unsigned int index);
      std::shared_ptr<xrt_core::device> getMCoreDevice();

      bool isXPR()           { return bXPR; }
      void setXPR(bool _xpr) { bXPR = _xpr; }
      std::string deviceDirectory;

      /* Path to the run directory for the current loaded bitstream for HW Emu
       * This directory contains the debug_ip_layout binary and simulation launch directories
       */
      std::string mRunDeviceBinDir;

      std::vector<std::string> parsedMsgs;

      bool isImported(unsigned int _bo)
      {
        if (mImportedBOs.find(_bo) != mImportedBOs.end())
          return true;
        return false;
      }
      //Get CU index from IP_LAYOUT section for corresponding kernel name
      int xclIPName2Index(const char *name);

      void fetchAndPrintMessages();
      std::mutex mPrintMessagesLock;
      // Restricted read/write on IP register space
      int xclRegWrite(uint32_t cu_index, uint32_t offset, uint32_t data);
      int xclRegRead(uint32_t cu_index, uint32_t offset, uint32_t *datap);

      bool device2xrt_rd_trans_cb(unsigned long int addr, void* const data_ptr,unsigned long int size);
      bool device2xrt_wr_trans_cb(unsigned long int addr, void const* data_ptr,unsigned long int size);
      bool device2xrt_irq_trans_cb(uint32_t,unsigned long int);

      std::string getSimulatorType(const std::string& binaryDirectory);
      void createPreSimScript(const std::string& wcfgFilePath, std::string& preSimScriptPath);
      std::string loadFileContentsToString(const std::string& path);
      void constructQueryTable();
      //CR-1120081
      void parseString(const std::string& simPath , const std::string& searchString);
      //CR-1120700
      int parseLog();
      void parseSimulateLog();
      void dumpDeadlockMessages();
      void setSimPath(std::string simPath) { sim_path = simPath; }
      std::string getSimPath () { return sim_path; }
      bool isHostOnlyBuffer(const struct xclemulation::drm_xocl_bo *bo) {
    	  if(xclemulation::config::getInstance()->isDisabledHostBUffer()) {
    		  return false;
    	  } else {
    		  return xclemulation::xocl_bo_host_only(bo);
    	  }
      }

      bool readEmuSettingsJsonFile(const std::string& emuSettingsFilePath);

    private:
      std::thread mMessengerThread;
      std::thread mHostMemAccessThread;
      std::atomic<bool> mMessengerThreadStarted;
      std::atomic<bool> mHostMemAccessThreadStarted;
      void messagesThread();
      void hostMemAccessThread();

      std::shared_ptr<xrt_core::device> mCoreDevice;
      bool simulator_started;
      uint64_t mRAMSize;
      size_t mCoalesceThreshold;
      void launchTempProcess() {};

      void initMemoryManager(std::list<xclemulation::DDRBank>& DDRBankList);
      //Mapped CU register space for xclRegRead/Write()
      int xclRegRW(bool rd, uint32_t cu_index, uint32_t offset, uint32_t *datap);

      std::vector<xclemulation::MemoryManager *> mDDRMemoryManager;
      xclemulation::MemoryManager* mDataSpace;
      std::list<xclemulation::DDRBank> mDdrBanks;
      std::map<uint64_t,uint64_t> mAddrMap;
      std::map<std::string,std::string> mBinaryDirectories;
      std::map<uint64_t , std::ofstream*> mOffsetInstanceStreamMap;

      //mutex to control parellel RPC calls
      std::mutex mtx;
      std::mutex mApiMtx;
      std::vector<Event> list_of_events[xdp::MAX_NUM_AIMS];
      unsigned int tracecount_calls;
      static std::map<std::string, std::string> mEnvironmentNameValueMap;

      void* ci_buf;
      call_packet_info ci_msg;

      response_packet_info ri_msg;
      void* ri_buf;
      size_t alloc_void(size_t new_size);

      void* buf;
      size_t buf_size;
      std::ofstream mLogStream;
      std::ofstream mGlobalInMemStream;
      std::ofstream mGlobalOutMemStream;
      static std::ofstream mDebugLogStream;
      static bool mFirstBinary;
      unsigned int binaryCounter;

      std::shared_ptr<unix_socket> sock;
      std::string deviceName;
      xclDeviceInfo2 mDeviceInfo;
      unsigned int mDeviceIndex;
      clock_t last_clk_time;
      bool mCloseAll;
      mem_model* mMemModel;
      bool bUnified;
      bool bXPR;
      //MemTopology topology;
      // HAL2 RELATED member variables start
      std::map<int, xclemulation::drm_xocl_bo*> mXoclObjMap;
      static unsigned int mBufferCount;
      // HAL2 RELATED member variables end
      exec_core* mCore;
      MBScheduler* mMBSch;
      hwemu::xocl_scheduler* m_scheduler;
      hwemu::xocl_xgq* m_xgq;

      // Information extracted from platform linker (for profile/debug)
      bool mIsDebugIpLayoutRead = false;
      bool mIsDeviceProfiling = false;
      uint32_t mMemoryProfilingNumberSlots;
      uint32_t mAccelProfilingNumberSlots;
      uint32_t mStreamProfilingNumberSlots;
      uint32_t mStallProfilingNumberSlots;
      uint64_t mPerfMonFifoCtrlBaseAddress;
      uint64_t mPerfMonFifoReadBaseAddress;
      uint64_t mTraceFunnelAddress;
      uint64_t mPerfMonBaseAddress[xdp::MAX_NUM_AIMS];
      uint64_t mAccelMonBaseAddress[xdp::MAX_NUM_AMS];
      uint64_t mStreamMonBaseAddress[xdp::MAX_NUM_ASMS];
      std::string mPerfMonSlotName[xdp::MAX_NUM_AIMS];
      std::string mAccelMonSlotName[xdp::MAX_NUM_AMS];
      std::string mStreamMonSlotName[xdp::MAX_NUM_ASMS];
      uint8_t mPerfmonProperties[xdp::MAX_NUM_AIMS];
      uint8_t mAccelmonProperties[xdp::MAX_NUM_AMS];
      uint8_t mStreamMonProperties[xdp::MAX_NUM_ASMS];
      std::vector<membank> mMembanks;
      static std::map<int, std::tuple<std::string,int,void*, unsigned int> > mFdToFileNameMap;
      std::list<std::tuple<uint64_t ,void*, std::map<uint64_t , uint64_t> > > mReqList;
      uint64_t mReqCounter;
      FeatureRomHeader mFeatureRom;
      boost::property_tree::ptree mPlatformData;
      std::map<key_type, std::string> mQueryTable;
      std::set<unsigned int > mImportedBOs;
      uint64_t mCuBaseAddress;
      bool     mVersalPlatform;
      //For Emulation specific messages on host from Device

      void closeMessengerThread();
      bool mIsTraceHubAvailable;
      uint32_t mCuIndx;
      std::map<std::string, uint64_t> mCURangeMap;
      std::string simulatorType;
      std::string sim_path;
      std::map<uint64_t, std::pair<void*, uint64_t> > mHostOnlyMemMap;
      unsigned int host_sptag_idx;
      bool mSimDontRun;
      nocddr_fastaccess_hwemu mNocFastAccess;
  };

  extern std::map<unsigned int, HwEmShim*> devices;
 }
#endif
