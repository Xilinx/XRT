#ifndef _XOCL_GEM_SHIM_H_
#define _XOCL_GEM_SHIM_H_

/**
 * Copyright (C) 2016-2018 Xilinx, Inc

 * Author(s): Umang Parekh
 *          : Sonal Santan
 *          : Ryan Radjabi
 * XOCL GEM HAL Driver layered on top of XOCL kernel driver
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

#include "driver/include/xclhal2.h"
#include "driver/xclng/include/xocl_ioctl.h"
#include "driver/xclng/include/mgmt-ioctl.h"
#include "driver/xclng/include/mgmt-reg.h"
#include "driver/xclng/include/qdma_ioctl.h"
#include <libdrm/drm.h>
#include <mutex>
#include <fstream>
#include <list>
#include <map>
#include <cassert>
#include <vector>
#include <linux/aio_abi.h>

namespace xocl {

const unsigned SHIM_USER_BAR = 0x0;
const unsigned SHIM_MGMT_BAR = 0x10000;
const uint64_t mNullAddr = 0xffffffffffffffffull;
const uint64_t mNullBO = 0xffffffff;

class shim
{
    struct ELARecord
    {
        unsigned mStartAddress;
        unsigned mEndAddress;
        unsigned mDataCount;
        std::streampos mDataPos;
        ELARecord() : mStartAddress(0), mEndAddress(0), mDataCount(0), mDataPos(0) {}
    };

    typedef std::list<ELARecord> ELARecordList;

public:
    ~shim();
    shim(unsigned index, const char *logfileName, xclVerbosityLevel verbosity);
    void init(unsigned index, const char *logfileName, xclVerbosityLevel verbosity);
    void readDebugIpLayout();
    static int xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char* tag, const char* format, va_list args1);
    // Raw read/write
    size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
    size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
    unsigned int xclAllocBO(size_t size, xclBOKind domain, unsigned flags);
    unsigned int xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags);
    void xclFreeBO(unsigned int boHandle);
    int xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek);
    int xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
    void *xclMapBO(unsigned int boHandle, bool write);
    int xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset);
    int xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size,
                  size_t dst_offset, size_t src_offset);

    int xclExportBO(unsigned int boHandle);
    unsigned int xclImportBO(int fd, unsigned flags);
    int xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties);

    // Bitstream/bin download
    int xclLoadXclBin(const xclBin *buffer);
    int xclLoadXclBinMgmt(const xclBin *buffer);
    int xclGetErrorStatus(xclErrorStatus *info);
    int xclGetDeviceInfo2(xclDeviceInfo2 *info);
    bool isGood() const;
    bool isGoodMgmt() const;
    static shim *handleCheck(void * handle);
    static shim *handleCheckMgmt(void * handle);
    int resetDevice(xclResetKind kind);
    int p2pEnable(bool enable, bool force);
    bool xclLockDevice();
    bool xclUnlockDevice();
    int xclReClock2(unsigned short region, const unsigned short *targetFreqMHz);
    int xclGetUsageInfo(xclDeviceUsage *info);

    int xclTestXSpi(int device_index);
    int xclBootFPGA();
    int xclRemoveAndScanFPGA();

    ssize_t xclUnmgdPwrite(unsigned flags, const void *buf, size_t count, uint64_t offset);
    ssize_t xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset);

    int xclGetSectionInfo(void *section_info, size_t *section_size, enum axlf_section_kind, int index);
    int xclReClockUser(unsigned short region, const unsigned short *targetFreqMHz);

    // Performance monitoring
    // Control
    double xclGetDeviceClockFreqMHz();
    double xclGetReadMaxBandwidthMBps();
    double xclGetWriteMaxBandwidthMBps();
    void xclSetProfilingNumberSlots(xclPerfMonType type, uint32_t numSlots);
    uint32_t getPerfMonNumberSlots(xclPerfMonType type);
    uint32_t getPerfMonProperties(xclPerfMonType type, uint32_t slotnum);
    void getPerfMonSlotName(xclPerfMonType type, uint32_t slotnum,
                            char* slotName, uint32_t length);
    size_t xclPerfMonClockTraining(xclPerfMonType type);
    void xclPerfMonConfigureDataflow(xclPerfMonType type, unsigned *ip_config);
    // Counters
    size_t xclPerfMonStartCounters(xclPerfMonType type);
    size_t xclPerfMonStopCounters(xclPerfMonType type);
    size_t xclPerfMonReadCounters(xclPerfMonType type, xclCounterResults& counterResults);

    //debug related
    uint32_t getCheckerNumberSlots(int type);
    uint32_t getIPCountAddrNames(int type, uint64_t *baseAddress, std::string * portNames,
                                    uint8_t *properties, uint8_t *majorVersions, uint8_t *minorVersions,
                                    size_t size);
    size_t xclDebugReadCounters(xclDebugCountersResults* debugResult);
    size_t xclDebugReadCheckers(xclDebugCheckersResults* checkerResult);
    size_t xclDebugReadStreamingCounters(xclStreamingDebugCountersResults* streamingResult);
    size_t xclDebugReadStreamingCheckers(xclDebugStreamingCheckersResults* streamingCheckerResult);
    size_t xclDebugReadAccelMonitorCounters(xclAccelMonitorCounterResults* samResult);


    // Trace
    size_t xclPerfMonStartTrace(xclPerfMonType type, uint32_t startTrigger);
    size_t xclPerfMonStopTrace(xclPerfMonType type);
    uint32_t xclPerfMonGetTraceCount(xclPerfMonType type);
    size_t xclPerfMonReadTrace(xclPerfMonType type, xclTraceResultsVector& traceVector);

    // APIs using sysfs information
    uint xclGetNumLiveProcesses();
    int xclGetSysfsPath(const char* subdev, const char* entry, char* sysfsPath, size_t size);

    // Experimental debug profile device data API
    int xclGetDebugProfileDeviceInfo(xclDebugProfileDeviceInfo* info);


    // Execute and interrupt abstraction
    int xclExecBuf(unsigned int cmdBO);
    int xclExecBuf(unsigned int cmdBO,size_t numdeps, unsigned int* bo_wait_list);
    int xclRegisterEventNotify(unsigned int userInterrupt, int fd);
    int xclExecWait(int timeoutMilliSec);
    int xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared) const;
    int xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex) const;

    int getBoardNumber( void ) { return mBoardNumber; }
    const char *getLogfileName( void ) { return mLogfileName; }
    xclVerbosityLevel getVerbosity( void ) { return mVerbosity; }

    // QDMA streaming APIs
    int xclCreateWriteQueue(xclQueueContext *q_ctx, uint64_t *q_hdl);
    int xclCreateReadQueue(xclQueueContext *q_ctx, uint64_t *q_hdl);
    int xclDestroyQueue(uint64_t q_hdl);
    void *xclAllocQDMABuf(size_t size, uint64_t *buf_hdl);
    int xclFreeQDMABuf(uint64_t buf_hdl);
    ssize_t xclWriteQueue(uint64_t q_hdl, xclQueueRequest *wr);
    ssize_t xclReadQueue(uint64_t q_hdl, xclQueueRequest *wr);
    int xclPollCompletion(int min_compl, int max_compl, xclReqCompletion *comps, int * actual, int timeout /*ms*/);

    // Temporary hack for xbflash use only
    char *xclMapMgmt(void) { return mMgtMap; }
    xclDeviceHandle xclOpenMgmt(unsigned deviceIndex, const char *logFileName, xclVerbosityLevel level);
    int xclMailbox(unsigned deviceIndex);
    int xclMailboxMgmt(unsigned deviceIndex);
    int xclMailboxMgmtPutID(unsigned deviceIndex, const char *id, const char *mbx_switch);
    int xclMailboxUserGetID(unsigned deviceIndex, char *id);

private:
    xclVerbosityLevel mVerbosity;
    std::ofstream mLogStream;
    int mUserHandle;
    int mMgtHandle;
    int mStreamHandle;
    char *mUserMap;
    int mBoardNumber;
    char *mMgtMap;
    bool mLocked;
    const char *mLogfileName;
    uint64_t mOffsets[XCL_ADDR_SPACE_MAX];
    xclDeviceInfo2 mDeviceInfo;
    ELARecordList mRecordList;
    uint32_t mMemoryProfilingNumberSlots;
    uint32_t mAccelProfilingNumberSlots;
    uint32_t mStallProfilingNumberSlots;
    uint32_t mStreamProfilingNumberSlots;
    std::string mDevUserName;

    bool zeroOutDDR();
    bool isXPR() const {
        return ((mDeviceInfo.mSubsystemId >> 12) == 4);
    }

    int dev_init();
    void dev_fini();

    int xclLoadAxlf(const axlf *buffer);
    int xclLoadAxlfMgmt(const axlf *buffer);
    void xclSysfsGetDeviceInfo(xclDeviceInfo2 *info);
    void xclSysfsGetUsageInfo(drm_xocl_usage_stat& stat);
    void xclSysfsGetErrorStatus(xclErrorStatus& stat);

    // Upper two denote PF, lower two bytes denote BAR
    // USERPF == 0x0
    // MGTPF == 0x10000

    int pcieBarRead(unsigned int pf_bar, unsigned long long offset, void* buffer, unsigned long long length);
    int pcieBarWrite(unsigned int pf_bar, unsigned long long offset, const void* buffer, unsigned long long length);
    int freezeAXIGate();
    int freeAXIGate();
    // PROM flashing
    int prepare_microblaze(unsigned startAddress, unsigned endAddress);
    int prepare(unsigned startAddress, unsigned endAddress);
    int program_microblaze(std::ifstream& mcsStream, const ELARecord& record);
    int program(std::ifstream& mcsStream, const ELARecord& record);
    int program(std::ifstream& mcsStream);
    int waitForReady_microblaze(unsigned code, bool verbose = true);
    int waitForReady(unsigned code, bool verbose = true);
    int waitAndFinish_microblaze(unsigned code, unsigned data, bool verbose = true);
    int waitAndFinish(unsigned code, unsigned data, bool verbose = true);

    //XSpi flashing.
    bool prepareXSpi();
    int programXSpi(std::ifstream& mcsStream, const ELARecord& record);
    int programXSpi(std::ifstream& mcsStream);
    bool waitTxEmpty();
    bool isFlashReady();
    //bool windDownWrites();
    bool bulkErase();
    bool sectorErase(unsigned Addr);
    bool writeEnable();
#if 0
    bool dataTransfer(bool read);
#endif
    bool readPage(unsigned addr, uint8_t readCmd = 0xff);
    bool writePage(unsigned addr, uint8_t writeCmd = 0xff);
    unsigned readReg(unsigned offset);
    int writeReg(unsigned regOffset, unsigned value);
    bool finalTransfer(uint8_t *sendBufPtr, uint8_t *recvBufPtr, int byteCount);
    bool getFlashId();
    //All remaining read /write register commands can be issued through this function.
    bool readRegister(unsigned commandCode, unsigned bytes);
    bool writeRegister(unsigned commandCode, unsigned value, unsigned bytes);
    bool select4ByteAddressMode();
    bool deSelect4ByteAddressMode();

    // Performance monitoring helper functions
    bool isDSAVersion(unsigned majorVersion, unsigned minorVersion, bool onlyThisVersion);
    unsigned getBankCount();
    signed cmpMonVersions(unsigned major1, unsigned minor1, unsigned major2, unsigned minor2);
    uint64_t getHostTraceTimeNsec();
    uint64_t getPerfMonBaseAddress(xclPerfMonType type, uint32_t slotNum);
    uint64_t getPerfMonFifoBaseAddress(xclPerfMonType type, uint32_t fifonum);
    uint64_t getPerfMonFifoReadBaseAddress(xclPerfMonType type, uint32_t fifonum);
    uint64_t getTraceFunnelAddress(xclPerfMonType type);
    uint32_t getPerfMonNumberSamples(xclPerfMonType type);
    uint32_t getPerfMonByteScaleFactor(xclPerfMonType type);
    uint8_t  getPerfMonShowIDS(xclPerfMonType type);
    uint8_t  getPerfMonShowLEN(xclPerfMonType type);
    uint32_t getPerfMonSlotStartBit(xclPerfMonType type, uint32_t slotnum);
    uint32_t getPerfMonSlotDataWidth(xclPerfMonType type, uint32_t slotnum);
    size_t resetFifos(xclPerfMonType type);
    uint32_t bin2dec(std::string str, int start, int number);
    uint32_t bin2dec(const char * str, int start, int number);
    std::string dec2bin(uint32_t n);
    std::string dec2bin(uint32_t n, unsigned bits);

    // Information extracted from platform linker
    bool mIsDebugIpLayoutRead = false;
    bool mIsDeviceProfiling = false;
    uint8_t mTraceFifoProperties = 0;
    uint64_t mPerfMonFifoCtrlBaseAddress = 0;
    uint64_t mPerfMonFifoReadBaseAddress = 0;
    uint64_t mTraceFunnelAddress = 0;
    uint64_t mPerfMonBaseAddress[XSPM_MAX_NUMBER_SLOTS] = {};
    uint64_t mAccelMonBaseAddress[XSAM_MAX_NUMBER_SLOTS] = {};
    uint64_t mStreamMonBaseAddress[XSSPM_MAX_NUMBER_SLOTS] = {};
    std::string mPerfMonSlotName[XSPM_MAX_NUMBER_SLOTS] = {};
    std::string mAccelMonSlotName[XSAM_MAX_NUMBER_SLOTS] = {};
    std::string mStreamMonSlotName[XSSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mPerfmonProperties[XSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mAccelmonProperties[XSAM_MAX_NUMBER_SLOTS] = {};
    uint8_t mStreammonProperties[XSSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mPerfmonMajorVersions[XSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mAccelmonMajorVersions[XSAM_MAX_NUMBER_SLOTS] = {};
    uint8_t mStreammonMajorVersions[XSSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mPerfmonMinorVersions[XSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mAccelmonMinorVersions[XSAM_MAX_NUMBER_SLOTS] = {};
    uint8_t mStreammonMinorVersions[XSSPM_MAX_NUMBER_SLOTS] = {};

    // QDMA AIO
    aio_context_t mAioContext;
    bool mAioEnabled;
}; /* shim */

} /* xocl */

#endif
