// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _SW_EMU_SHIM_H_
#define _SW_EMU_SHIM_H_

#include "config.h"
#include "em_defines.h"
#include "memorymanager.h"
#include "rpc_messages.pb.h"

#include "core/include/xdp/common.h"
#include "core/include/xdp/counters.h"
#include "core/include/xdp/trace.h"

#include "swscheduler.h"
#include "unix_socket.h"
#include "xclbin.h"
#include "xcl_api_macros.h"
#include "xcl_macros.h"

#include "core/common/api/xclbin_int.h"
#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/scheduler.h"
#include "core/common/query_requests.h"
#include "core/common/xrt_profiling.h"
#include "core/include/experimental/xrt_hw_context.h"
#include "core/include/experimental/xrt_xclbin.h"

#include <fcntl.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <atomic>
#include <thread>
#include <tuple>

#ifndef _WINDOWS
#include <dlfcn.h>
#endif

namespace xclcpuemhal2
{
  using key_type = xrt_core::query::key_type;
  //8GB MEMSIZE to access the MMAP FILE
  const uint64_t MEMSIZE = 0x0000000400000000;
  const auto endOfSimulationString = "received request to end simulation from connected initiator";

  // XDMA Shim
  class CpuemShim
  {
  public:
    static const unsigned TAG;
    static const unsigned CONTROL_AP_START;
    static const unsigned CONTROL_AP_DONE;
    static const unsigned CONTROL_AP_IDLE;
    static const unsigned CONTROL_AP_CONTINUE;

  private:
    // This is a hidden signature of this class and helps in preventing
    // user errors when incorrect pointers are passed in as handles.
    const unsigned mTag;

  private:
    // Helper functions - added for kernel debug
    int dumpXML(const xclBin *header, std::string &fileLocation);
    bool parseIni(unsigned int &debugPort);
    void getCuRangeIdx();
    static std::map<std::string, std::string> mEnvironmentNameValueMap;

  public:
    // HAL2 RELATED member functions start
    unsigned int xclAllocBO(size_t size, int unused, unsigned flags);
    uint64_t xoclCreateBo(xclemulation::xocl_create_bo *info);
    void *xclMapBO(unsigned int boHandle, bool write);
    int xclUnmapBO(unsigned int boHandle, void *addr);
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
    static int xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char *tag, const char *format, va_list args1);

    xclemulation::drm_xocl_bo *xclGetBoByHandle(unsigned int boHandle);
    inline unsigned short xocl_ddr_channel_count();
    inline unsigned long long xocl_ddr_channel_size();
    // HAL2 RELATED member functions end

    //Configuration
    void xclOpen(const char *logfileName);
    void setDriverVersion(const std::string& version);
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
    uint64_t xclAllocDeviceBuffer2(size_t &size, xclMemoryDomains domain, unsigned flags, bool p2pBuffer, std::string &sFileName);

    void xclFreeDeviceBuffer(uint64_t buf);
    size_t xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek);
    size_t xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip);

    // Performance monitoring
    // Control
    double xclGetDeviceClockFreqMHz();
    double xclGetHostReadMaxBandwidthMBps();
    double xclGetHostWriteMaxBandwidthMBps();
    double xclGetKernelReadMaxBandwidthMBps();
    double xclGetKernelWriteMaxBandwidthMBps();
    void xclSetProfilingNumberSlots(xdp::MonitorType type, uint32_t numSlots);
    size_t xclPerfMonClockTraining(xdp::MonitorType type);
    // Counters
    size_t xclPerfMonStartCounters(xdp::MonitorType type);
    size_t xclPerfMonStopCounters(xdp::MonitorType type);
    size_t xclPerfMonReadCounters(xdp::MonitorType type, xdp::CounterResults &counterResults);
    // Trace
    size_t xclPerfMonStartTrace(xdp::MonitorType type, uint32_t startTrigger);
    size_t xclPerfMonStopTrace(xdp::MonitorType type);
    uint32_t xclPerfMonGetTraceCount(xdp::MonitorType type);
    size_t xclPerfMonReadTrace(xdp::MonitorType type, xdp::TraceEventsVector &traceVector);

    // Sanity checks
    int xclGetDeviceInfo2(xclDeviceInfo2 *info);
    static unsigned xclProbe();
    void fillDeviceInfo(xclDeviceInfo2 *dest, xclDeviceInfo2 *src);
    void saveDeviceProcessOutput();

    void set_messagesize(unsigned int messageSize) { message_size = messageSize; }
    unsigned int get_messagesize() { return message_size; }

    ~CpuemShim();
    CpuemShim(unsigned int deviceIndex, xclDeviceInfo2 &info, std::list<xclemulation::DDRBank> &DDRBankList, bool bUnified,
              bool bXPR, FeatureRomHeader &featureRom, const boost::property_tree::ptree &platformData);

    static CpuemShim *handleCheck(void *handle);
    bool isGood() const;

    int xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared);
    int xclExecWait(int timeoutMilliSec);
    int xclExecBuf(unsigned int cmdBO);
    int xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex);
    //Get CU index from IP_LAYOUT section for corresponding kernel name
    int xclIPName2Index(const char *name);

    bool isValidCu(uint32_t cu_index);
    uint64_t getCuAddRange(uint32_t cu_index);
    std::string getDeviceProcessLogPath();
    bool isValidOffset(uint32_t offset, uint64_t cuAddRange);
    int xclRegRW(bool rd, uint32_t cu_index, uint32_t offset, uint32_t *datap);
    int xclRegRead(uint32_t cu_index, uint32_t offset, uint32_t *datap);
    int xclRegWrite(uint32_t cu_index, uint32_t offset, uint32_t data);
    bool isImported(unsigned int _bo)
    {
      if (mImportedBOs.find(_bo) != mImportedBOs.end())
        return true;
      return false;
    }
    struct exec_core *getExecCore() { return mCore; }
    SWScheduler *getScheduler() { return mSWSch; }

    // New API's for m2m and no-dma
    void constructQueryTable();
    int deviceQuery(key_type queryKey);
    void messagesThread();

    //******************************* XRT Graph API's **************************************************//
    /**
      * xrtGraphInit() - Initialize graph
      *
      * @gh:             Handle to graph previously opened with xrtGraphOpen.
      * Return:          0 on success, -1 on error
      *
      * Note: Run by enable tiles and disable tile reset
      */
    int
    xrtGraphInit(void *gh);

    /**
      * xrtGraphRun() - Start a graph execution
      *
      * @gh:             Handle to graph previously opened with xrtGraphOpen.
      * @iterations:     The run iteration to update to graph. 0 for infinite.
      * Return:          0 on success, -1 on error
      *
      * Note: Run by enable tiles and disable tile reset
      */
    int
    xrtGraphRun(void *gh, uint32_t iterations);

    /**
      * xrtGraphWait() -  Wait a given AIE cycle since the last xrtGraphRun and
      *                   then stop the graph. If cycle is 0, busy wait until graph
      *                   is done. If graph already run more than the given
      *                   cycle, stop the graph immediateley.
      *
      * @gh:              Handle to graph previously opened with xrtGraphOpen.
      *
      * Return:          0 on success, -1 on error.
      *
      * Note: This API with non-zero AIE cycle is for graph that is running
      * forever or graph that has multi-rate core(s).
      */
    int
    xrtGraphWait(void *gh);

    /**
      * xrtGraphEnd() - Wait a given AIE cycle since the last xrtGraphRun and
      *                 then end the graph. busy wait until graph
      *                 is done before end the graph. If graph already run more
      *                 than the given cycle, stop the graph immediately and end it.
      *
      * @gh:              Handle to graph previously opened with xrtGraphOpen.
      *
      * Return:          0 on success, -1 on timeout.
      *
      * Note: This API with non-zero AIE cycle is for graph that is running
      * forever or graph that has multi-rate core(s).
      */
    int
    xrtGraphEnd(void *gh);

    /**
      * xrtGraphUpdateRTP() - Update RTP value of port with hierarchical name
      *
      * @gh:              Handle to graph previously opened with xrtGraphOpen.
      * @hierPathPort:    hierarchial name of RTP port.
      * @buffer:          pointer to the RTP value.
      * @size:            size in bytes of the RTP value.
      *
      * Return:          0 on success, -1 on error.
      */
    int
    xrtGraphUpdateRTP(void *gh, const char *hierPathPort, const char *buffer, size_t size);

    /**
      * xrtGraphUpdateRTP() - Read RTP value of port with hierarchical name
      *
      * @gh:              Handle to graph previously opened with xrtGraphOpen.
      * @hierPathPort:    hierarchial name of RTP port.
      * @buffer:          pointer to the buffer that RTP value is copied to.
      * @size:            size in bytes of the RTP value.
      *
      * Return:          0 on success, -1 on error.
      *
      * Note: Caller is reponsible for allocating enough memory for RTP value
      *       being copied to.
      */
    int
    xrtGraphReadRTP(void *gh, const char *hierPathPort, char *buffer, size_t size);

    /**
      * xrtSyncBOAIENB() - Transfer data between DDR and Shim DMA channel
      *
      * @bo:           BO obj.
      * @gmioName:        GMIO port name
      * @dir:             GM to AIE or AIE to GM
      * @size:            Size of data to synchronize
      * @offset:          Offset within the BO
      *
      * Return:          0 on success, or appropriate error number.
      *
      * Synchronize the buffer contents between GMIO and AIE.
      * Note: Upon return, the synchronization is submitted or error out
      */
    int
    xrtSyncBOAIENB(xrt::bo &bo, const char *gmioname, enum xclBOSyncDirection dir, size_t size, size_t offset);

    /**
      * xrtGMIOWait() - Wait a shim DMA channel to be idle for a given GMIO port
      *
      * @gmioName:        GMIO port name
      *
      * Return:          0 on success, or appropriate error number.
      */
    int
    xrtGMIOWait(const char *gmioname);

    /**
      * xrtGraphResume() - Resume a suspended graph.
      *
      * Resume graph execution which was paused by suspend() or wait(cycles) APIs
      */
    int
    xrtGraphResume(void *gh);

    /**
      * xrtGraphTimedEnd() - Wait a given AIE cycle since the last xrtGraphRun and
      *                 then end the graph. If cycle is 0, busy wait until graph
      *                 is done before end the graph. If graph already run more
      *                 than the given cycle, stop the graph immediately and end it.
      */
    int
    xrtGraphTimedEnd(void *gh, uint64_t cycle);

    /**
      * xrtGraphTimedWait() -  Wait a given AIE cycle since the last xrtGraphRun and
      *                   then stop the graph. If cycle is 0, busy wait until graph
      *                   is done. If graph already run more than the given
      *                   cycle, stop the graph immediateley.
      */
    int
    xrtGraphTimedWait(void *gh, uint64_t cycle);

    // //******************************* XRT Graph API's **************************************************//
    // /**
    // * xrtGraphInit() - Initialize graph
    // *
    // * @gh:             Handle to graph previously opened with xrtGraphOpen.
    // * Return:          0 on success, -1 on error
    // *
    // * Note: Run by enable tiles and disable tile reset
    // */
    // int
    //   xrtGraphInit(void * gh);
    //
    // /**
    // * xrtGraphRun() - Start a graph execution
    // *
    // * @gh:             Handle to graph previously opened with xrtGraphOpen.
    // * @iterations:     The run iteration to update to graph. 0 for infinite.
    // * Return:          0 on success, -1 on error
    // *
    // * Note: Run by enable tiles and disable tile reset
    // */
    // int
    //   xrtGraphRun(void * gh, uint32_t iterations);
    //
    // /**
    // * xrtGraphWait() -  Wait a given AIE cycle since the last xrtGraphRun and
    // *                   then stop the graph. If cycle is 0, busy wait until graph
    // *                   is done. If graph already run more than the given
    // *                   cycle, stop the graph immediateley.
    // *
    // * @gh:              Handle to graph previously opened with xrtGraphOpen.
    // *
    // * Return:          0 on success, -1 on error.
    // *
    // * Note: This API with non-zero AIE cycle is for graph that is running
    // * forever or graph that has multi-rate core(s).
    // */
    // int
    //   xrtGraphWait(void * gh);
    //
    // /**
    // * xrtGraphEnd() - Wait a given AIE cycle since the last xrtGraphRun and
    // *                 then end the graph. busy wait until graph
    // *                 is done before end the graph. If graph already run more
    // *                 than the given cycle, stop the graph immediately and end it.
    // *
    // * @gh:              Handle to graph previously opened with xrtGraphOpen.
    // *
    // * Return:          0 on success, -1 on timeout.
    // *
    // * Note: This API with non-zero AIE cycle is for graph that is running
    // * forever or graph that has multi-rate core(s).
    // */
    // int
    //   xrtGraphEnd(void * gh);
    //
    // /**
    // * xrtGraphUpdateRTP() - Update RTP value of port with hierarchical name
    // *
    // * @gh:              Handle to graph previously opened with xrtGraphOpen.
    // * @hierPathPort:    hierarchial name of RTP port.
    // * @buffer:          pointer to the RTP value.
    // * @size:            size in bytes of the RTP value.
    // *
    // * Return:          0 on success, -1 on error.
    // */
    // int
    //   xrtGraphUpdateRTP(void * gh, const char *hierPathPort, const char *buffer, size_t size);
    //
    // /**
    // * xrtGraphUpdateRTP() - Read RTP value of port with hierarchical name
    // *
    // * @gh:              Handle to graph previously opened with xrtGraphOpen.
    // * @hierPathPort:    hierarchial name of RTP port.
    // * @buffer:          pointer to the buffer that RTP value is copied to.
    // * @size:            size in bytes of the RTP value.
    // *
    // * Return:          0 on success, -1 on error.
    // *
    // * Note: Caller is reponsible for allocating enough memory for RTP value
    // *       being copied to.
    // */
    // int
    //   xrtGraphReadRTP(void * gh, const char *hierPathPort, char *buffer, size_t size);

    ////////////////////////////////////////////////////////////////
    // Internal SHIM APIs
    ////////////////////////////////////////////////////////////////
    // aka xclOpenContextByName
    xrt_core::cuidx_type
    open_cu_context(const xrt::hw_context &hwctx, const std::string &cuname);
    void
    close_cu_context(const xrt::hw_context& hwctx, xrt_core::cuidx_type cuidx);
  private:
    std::shared_ptr<xrt_core::device> mCoreDevice;
    std::mutex mMemManagerMutex;

    // Performance monitoring helper functions
    bool isDSAVersion(double checkVersion, bool onlyThisVersion);
    uint64_t getHostTraceTimeNsec();
    uint64_t getPerfMonBaseAddress(xdp::MonitorType type);
    uint64_t getPerfMonFifoBaseAddress(xdp::MonitorType type, uint32_t fifonum);
    uint64_t getPerfMonFifoReadBaseAddress(xdp::MonitorType type, uint32_t fifonum);
    uint32_t getPerfMonNumberSlots(xdp::MonitorType type);
    uint32_t getPerfMonNumberSamples(xdp::MonitorType type);
    uint32_t getPerfMonNumberFifos(xdp::MonitorType type);
    uint32_t getPerfMonByteScaleFactor(xdp::MonitorType type);
    uint8_t getPerfMonShowIDS(xdp::MonitorType type);
    uint8_t getPerfMonShowLEN(xdp::MonitorType type);
    size_t resetFifos(xdp::MonitorType type);
    uint32_t bin2dec(std::string str, int start, int number);
    uint32_t bin2dec(const char *str, int start, int number);
    std::string dec2bin(uint32_t n);
    std::string dec2bin(uint32_t n, unsigned bits);
    void closeMessengerThread();

    std::mutex mtx;
    unsigned int message_size;
    bool simulator_started;

    std::ofstream mLogStream;
    xclVerbosityLevel mVerbosity;

    std::vector<std::string> mTempdlopenfilenames;
    std::string deviceName;
    std::string deviceDirectory;
    // a thread variable which calls messagesThread,
    // messagesThread is a joinable thread used to display any messages seen in device_process.log
    std::thread mMessengerThread;
    std::list<xclemulation::DDRBank> mDdrBanks;
    std::map<uint64_t, std::pair<std::string, unsigned int>> kernelArgsInfo;
    xclDeviceInfo2 mDeviceInfo;

    void launchDeviceProcess(bool debuggable, std::string &binDir);
    void launchTempProcess();
    void initMemoryManager(std::list<xclemulation::DDRBank> &DDRBankList);
    std::vector<xclemulation::MemoryManager *> mDDRMemoryManager;

    void *ci_buf;
    call_packet_info ci_msg;

    response_packet_info ri_msg;
    void *ri_buf;
    size_t alloc_void(size_t new_size);

    void *buf;
    size_t buf_size;
    unsigned int binaryCounter;
    unix_socket *sock;
    unix_socket *aiesim_sock;

    uint64_t mRAMSize;
    size_t mCoalesceThreshold;
    unsigned int mDeviceIndex;
    bool mCloseAll;

    std::mutex mProcessLaunchMtx;
    std::mutex mApiMtx;
    static bool mFirstBinary;
    bool bUnified;
    bool bXPR;
    // HAL2 RELATED member variables start
    std::map<int, xclemulation::drm_xocl_bo *> mXoclObjMap;
    static unsigned int mBufferCount;
    static std::map<int, std::tuple<std::string, uint64_t, void *>> mFdToFileNameMap;
    // HAL2 RELATED member variables end
    std::list<std::tuple<uint64_t, void *, std::map<uint64_t, uint64_t>>> mReqList;
    uint64_t mReqCounter;
    FeatureRomHeader mFeatureRom;
    boost::property_tree::ptree mPlatformData;
    std::map<key_type, std::string> mQueryTable;
    std::map<std::string, uint64_t> mCURangeMap;
    xrt::xclbin m_xclbin;
    std::set<unsigned int> mImportedBOs;
    exec_core *mCore;
    SWScheduler *mSWSch;
    bool mIsKdsSwEmu;
    std::atomic<bool> mIsDeviceProcessStarted;
  };

  class GraphType
  {
    // Core device to which the graph belongs.  The core device
    // has been loaded with an xclbin from which meta data can
    // be extracted
  public:
    GraphType(xclcpuemhal2::CpuemShim *handle, const char *graph)
    {
      _deviceHandle = handle;
      //_xclbin_uuid = xclbin_uuid;
      _graph = graph;
      graphHandle = mGraphHandle++;
      _state = graph_state::stop;
      _name = "";
      _startTime = 0;
    }
    xclcpuemhal2::CpuemShim *getDeviceHandle() { return _deviceHandle; }
    const char *getGraphName() { return _graph; }
    unsigned int getGraphHandle() { return graphHandle; }

  private:
    xclcpuemhal2::CpuemShim *_deviceHandle;
    //const uuid_t _xclbin_uuid;
    const char *_graph;
    unsigned int graphHandle;
    enum class graph_state : unsigned short
    {
      stop = 0,
      reset = 1,
      running = 2,
      suspend = 3,
      end = 4,
    };
    graph_state _state;
    std::string _name;
    uint64_t _startTime;
    /* This is the collections of rtps that are used. */
    std::vector<std::string> rtps;
    static unsigned int mGraphHandle;
  };
  extern std::map<unsigned int, CpuemShim *> devices;

  // sParseLog structure parses a file named mFileName and looks for a matchString
  // On successfull match, print the line to the console
  // Currently, we are using this structure to parse the external IO file that is
  // generated by the deviceProcess during SW EMU.

  struct sParseLog
  {
    std::ifstream file;
    std::string mFileName;
    std::atomic<bool> mFileExists;
    CpuemShim *mCpuShimPtr;

    sParseLog(CpuemShim *iPtr, const std::string &iDeviceLog)
        : mFileName(iDeviceLog), mFileExists{false}, mCpuShimPtr(iPtr)
    {
    }

    //**********************************************************************************//
    /**
    * closeApplicationOnMagicStrFound(std::string&) - Searches for a matchString in a file .
    * On a successfull match, it prints a user visible message on the console and exits the application.
    *
    * @matchString:    string to match
    *
    */
    void closeApplicationOnMagicStrFound(const std::string &matchString)
    {
      std::string line;
      while (std::getline(file, line))
      {
        if (line.find(matchString) != std::string::npos)
        {
          std::cout << "Received request to end the application. Exiting the application." << std::endl;
          mCpuShimPtr->xclClose();
        }
      }
    }

    //**********************************************************************************//
    /**
    * parseLog() - Checks for file existence and calls closeApplication.
    *
    */
    void parseLog()
    {
      if (!mFileExists)
      {
        if (boost::filesystem::exists(mFileName))
        {
          file.open(mFileName,std::ios::in);
          if (file.is_open())
            mFileExists = true;
        }
      }

      if (mFileExists)
        closeApplicationOnMagicStrFound(endOfSimulationString);
    }
  };
}

#endif
