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

#ifndef _SW_EMU_SHIM_H_
#define _SW_EMU_SHIM_H_

#include "unix_socket.h"
#include "config.h"
#include "em_defines.h"
#include "memorymanager.h"
#include "rpc_messages.pb.h"

#include "xclperf.h"
#include "xcl_api_macros.h"
#include "xcl_macros.h"
#include "xclbin.h"
#include "core/common/scheduler.h"
#include "core/common/message.h"

#include <stdarg.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>
#include <tuple>
#include <sys/wait.h>
#ifndef _WINDOWS
#include <dlfcn.h>
#endif


namespace xclcpuemhal2 {
  // XDMA Shim
  class CpuemShim {
    public:
      static const unsigned TAG;
      static const unsigned CONTROL_AP_START;
      static const unsigned CONTROL_AP_DONE;
      static const unsigned CONTROL_AP_IDLE;

  private:
      // This is a hidden signature of this class and helps in preventing
      // user errors when incorrect pointers are passed in as handles.
      const unsigned mTag;

  private:
      // Helper functions - added for kernel debug
      int dumpXML(const xclBin* header, std::string& fileLocation) ;
      bool parseIni(unsigned int& debugPort) ;
      static std::map<std::string, std::string> mEnvironmentNameValueMap;
  public:
      // HAL2 RELATED member functions start
      unsigned int xclAllocBO(size_t size, xclBOKind domain, unsigned flags);
      int xoclCreateBo(xclemulation::xocl_create_bo *info);
      void* xclMapBO(unsigned int boHandle, bool write);
      int xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset); 
      unsigned int xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags);
      int xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties);
      size_t xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek);
      size_t xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
      void xclFreeBO(unsigned int boHandle);
      //P2P buffer support
      int xclExportBO(unsigned int boHandle); 
      unsigned int xclImportBO(int boGlobalHandle, unsigned flags);
      int xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset);
      static int xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char* tag, const char* format, va_list args1);
      

      xclemulation::drm_xocl_bo* xclGetBoByHandle(unsigned int boHandle);
      inline unsigned short xocl_ddr_channel_count();
      inline unsigned long long xocl_ddr_channel_size();
      // HAL2 RELATED member functions end 

      //Configuration
      void xclOpen(const char* logfileName);
      int xclLoadXclBin(const xclBin *buffer);
      //int xclLoadBitstream(const char *fileName);
      int xclUpgradeFirmware(const char *fileName);
      int xclBootFPGA();
      void xclClose();
      void resetProgram(bool callingFromClose = false);

      // Raw read/write
      size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
      size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);

      // Buffer management
      uint64_t xclAllocDeviceBuffer(size_t size);
      uint64_t xclAllocDeviceBuffer2(size_t& size, xclMemoryDomains domain, unsigned flags, bool p2pBuffer, std::string &sFileName);

      void xclFreeDeviceBuffer(uint64_t buf);
      size_t xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek);
      size_t xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip);

      // Performance monitoring
      // Control
      double xclGetDeviceClockFreqMHz();
      double xclGetReadMaxBandwidthMBps();
      double xclGetWriteMaxBandwidthMBps();
      void xclSetProfilingNumberSlots(xclPerfMonType type, uint32_t numSlots);
      size_t xclPerfMonClockTraining(xclPerfMonType type);
      // Counters
      size_t xclPerfMonStartCounters(xclPerfMonType type);
      size_t xclPerfMonStopCounters(xclPerfMonType type);
      size_t xclPerfMonReadCounters(xclPerfMonType type, xclCounterResults& counterResults);
      // Trace
      size_t xclPerfMonStartTrace(xclPerfMonType type, uint32_t startTrigger);
      size_t xclPerfMonStopTrace(xclPerfMonType type);
      uint32_t xclPerfMonGetTraceCount(xclPerfMonType type);
      size_t xclPerfMonReadTrace(xclPerfMonType type, xclTraceResultsVector& traceVector);

      // Sanity checks
      int xclGetDeviceInfo2(xclDeviceInfo2 *info);
      static unsigned xclProbe();
      void fillDeviceInfo(xclDeviceInfo2* dest, xclDeviceInfo2* src);
      void saveDeviceProcessOutput();
      
      void set_messagesize( unsigned int messageSize ) { message_size = messageSize; }
      unsigned int get_messagesize(){ return message_size; }


      ~CpuemShim();
      CpuemShim(unsigned int deviceIndex, xclDeviceInfo2 &info, std::list<xclemulation::DDRBank>& DDRBankList, bool bUnified, bool bXPR, FeatureRomHeader &featureRom );

      static CpuemShim *handleCheck(void *handle);
      bool isGood() const;

      //QDMA Support
      int xclCreateWriteQueue(xclQueueContext *q_ctx, uint64_t *q_hdl);
      int xclCreateReadQueue(xclQueueContext *q_ctx, uint64_t *q_hdl);
      int xclDestroyQueue(uint64_t q_hdl);
      void *xclAllocQDMABuf(size_t size, uint64_t *buf_hdl);
      int xclFreeQDMABuf(uint64_t buf_hdl);
      ssize_t xclWriteQueue(uint64_t q_hdl, xclQueueRequest *wr);
      ssize_t xclReadQueue(uint64_t q_hdl, xclQueueRequest *wr);
      int xclPollCompletion(int min_compl, int max_compl, xclReqCompletion *comps, int* actual, int timeout);


    private:
      std::mutex mMemManagerMutex;

      // Performance monitoring helper functions
      bool isDSAVersion(double checkVersion, bool onlyThisVersion);
      uint64_t getHostTraceTimeNsec();
      uint64_t getPerfMonBaseAddress(xclPerfMonType type);
      uint64_t getPerfMonFifoBaseAddress(xclPerfMonType type, uint32_t fifonum);
      uint64_t getPerfMonFifoReadBaseAddress(xclPerfMonType type, uint32_t fifonum);
      uint32_t getPerfMonNumberSlots(xclPerfMonType type);
      uint32_t getPerfMonNumberSamples(xclPerfMonType type);
      uint32_t getPerfMonNumberFifos(xclPerfMonType type);
      uint32_t getPerfMonByteScaleFactor(xclPerfMonType type);
      uint8_t  getPerfMonShowIDS(xclPerfMonType type);
      uint8_t  getPerfMonShowLEN(xclPerfMonType type);
      size_t resetFifos(xclPerfMonType type);
      uint32_t bin2dec(std::string str, int start, int number);
      uint32_t bin2dec(const char * str, int start, int number);
      std::string dec2bin(uint32_t n);
      std::string dec2bin(uint32_t n, unsigned bits);

      std::mutex mtx;
      unsigned int message_size;
      bool simulator_started;

      std::ofstream mLogStream;
      xclVerbosityLevel mVerbosity;

      std::vector<std::string> mTempdlopenfilenames;
      std::string deviceName;
      std::string deviceDirectory;
      std::list<xclemulation::DDRBank> mDdrBanks;
      std::map<uint64_t,std::pair<std::string,unsigned int>> kernelArgsInfo;
      xclDeviceInfo2 mDeviceInfo;

      void launchDeviceProcess(bool debuggable, std::string& binDir);
      void launchTempProcess();
      void initMemoryManager(std::list<xclemulation::DDRBank>& DDRBankList);
      std::vector<xclemulation::MemoryManager *> mDDRMemoryManager;

      void* ci_buf;
      call_packet_info ci_msg;

      response_packet_info ri_msg;
      void* ri_buf;
      size_t alloc_void(size_t new_size);

      void* buf;
      size_t buf_size;
      unsigned int binaryCounter;
      unix_socket* sock;


      uint64_t mRAMSize;
      size_t mCoalesceThreshold;
      int mDSAMajorVersion;
      int mDSAMinorVersion;
      unsigned int mDeviceIndex;
      bool mCloseAll;
      
      std::mutex mProcessLaunchMtx;
      std::mutex mApiMtx;
      static bool mFirstBinary;
      bool bUnified;
      bool bXPR;
      // HAL2 RELATED member variables start
      std::map<int, xclemulation::drm_xocl_bo*> mXoclObjMap;
      static unsigned int mBufferCount;
      static std::map<int, std::tuple<std::string,int,void*> > mFdToFileNameMap;
      // HAL2 RELATED member variables end 
      std::list<std::tuple<uint64_t ,void*, std::map<uint64_t , uint64_t> > > mReqList;
      uint64_t mReqCounter;
      FeatureRomHeader mFeatureRom;

  };
  
  extern std::map<unsigned int, CpuemShim*> devices;
}

#endif


