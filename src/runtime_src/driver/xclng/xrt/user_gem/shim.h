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

// This list will get populated in xclProbe
// 0 -> /dev/dri/renderD129
// 1 -> /dev/dri/renderD130
static const std::map<std::string, std::string> deviceOld2NewNameMap = {
    std::pair<std::string, std::string>("xilinx:adm-pcie-7v3:1ddr:3.0", "xilinx_adm-pcie-7v3_1ddr_3_0"),
    std::pair<std::string, std::string>("xilinx:adm-pcie-8k5:2ddr:4.0", "xilinx_adm-pcie-8k5_2ddr_4_0"),
    std::pair<std::string, std::string>("xilinx:adm-pcie-ku3:2ddr-xpr:4.0", "xilinx_adm-pcie-ku3_2ddr-xpr_4_0"),
    std::pair<std::string, std::string>("xilinx:adm-pcie-ku3:2ddr:4.0", "xilinx_adm-pcie-ku3_2ddr_4_0"),
    std::pair<std::string, std::string>("xilinx:aws-vu9p-f1:4ddr-xpr-2pr:4.0", "xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0"),
    std::pair<std::string, std::string>("xilinx:kcu1500:4ddr-xpr:4.0", "xilinx_kcu1500_4ddr-xpr_4_0"),
    std::pair<std::string, std::string>("xilinx:kcu1500:4ddr-xpr:4.3", "xilinx_kcu1500_4ddr-xpr_4_3"),
    std::pair<std::string, std::string>("xilinx:vcu1525:4ddr-xpr:4.2", "xilinx_vcu1525_4ddr-xpr_4_2"),
    std::pair<std::string, std::string>("xilinx:xil-accel-rd-ku115:4ddr-xpr:4.0", "xilinx_xil-accel-rd-ku115_4ddr-xpr_4_0"),
    std::pair<std::string, std::string>("xilinx:xil-accel-rd-vu9p-hp:4ddr-xpr:4.2", "xilinx_xil-accel-rd-vu9p-hp_4ddr-xpr_4_2"),
    std::pair<std::string, std::string>("xilinx:xil-accel-rd-vu9p:4ddr-xpr-xare:4.6", "xilinx_xil-accel-rd-vu9p_4ddr-xpr-xare_4_6"),
    std::pair<std::string, std::string>("xilinx:xil-accel-rd-vu9p:4ddr-xpr:4.0", "xilinx_xil-accel-rd-vu9p_4ddr-xpr_4_0"),
    std::pair<std::string, std::string>("xilinx:xil-accel-rd-vu9p:4ddr-xpr:4.2", "xilinx_xil-accel-rd-vu9p_4ddr-xpr_4_2"),
    std::pair<std::string, std::string>("xilinx:zc706:linux-uart:1.0", "xilinx_zc706_linux-uart_1_0"),
    std::pair<std::string, std::string>("xilinx:zcu102:1HP:1.1", "xilinx_zcu102_1HP_1_1"),
    std::pair<std::string, std::string>("xilinx:zcu102:4HP:1.2", "xilinx_zcu102_4HP_1_2")
};
const std::string newDeviceName(const std::string& name);
unsigned numClocks(const std::string& name);
struct AddressRange;
std::ostream& operator<< (std::ostream &strm, const AddressRange &rng);

/**
 * Simple tuple struct to store non overlapping address ranges: address and size
 */
struct AddressRange : public std::pair<uint64_t, size_t>
{
    // size will be zero when we are looking up an address that was passed by the user
    AddressRange(uint64_t addr, size_t size = 0) : std::pair<uint64_t, size_t>(std::make_pair(addr, size)) {
        //std::cout << "CTOR(" << addr << ',' << size << ")\n";
    }
    AddressRange(AddressRange && rhs) : std::pair<uint64_t, size_t>(std::move(rhs)) {
        //std::cout << "MOVE CTOR(" << rhs.first << ',' << rhs.second << ")\n";
    }

    AddressRange(const AddressRange &rhs) = delete;
    AddressRange& operator=(const AddressRange &rhs) = delete;

    // Comparison operator is useful when using AddressRange as a key in std::map
    // Note one operand in the comparator may have only the address without the size
    // However both operands in the comparator will not have zero size
    bool operator < (const AddressRange& other) const {
        //std::cout << *this << " < " << other << "\n";
        if ((this->second != 0) && (other.second != 0))
            // regular ranges
            return (this->first < other.first);
        if (other.second == 0)
            // second range just has an address
            // (1000, 100) < (1200, 0)
            // (1000, 100) < (1100, 0) first range ends at 1099
            return ((this->first + this->second) <= other.first);
        assert(this->second == 0);
        // this range just has an address
        // (1100, 0) < (1200, 100)
        return (this->first < other.first);
    }
}; /* AddressRange */

/**
 * Simple map of address range to its bo handle and mapped virtual address
 */
static const std::pair<unsigned, char *> mNullValue = std::make_pair(0xffffffff, nullptr);

class RangeTable
{
    std::map<AddressRange, std::pair<unsigned, char *>> mTable;
    mutable std::mutex mMutex;
public:
    void insert(uint64_t addr, size_t size, const std::pair<unsigned, char *> &bo) {
        // assert(find(addr) == 0xffffffff);
        std::lock_guard<std::mutex> lock(mMutex);
        mTable[AddressRange(addr, size)] = bo;
    }

    std::pair<unsigned, char *> erase(uint64_t addr) {
        std::lock_guard<std::mutex> lock(mMutex);
        std::map<AddressRange, std::pair<unsigned, char *>>::const_iterator i = mTable.find(AddressRange(addr));
        if (i == mTable.end())
            return mNullValue;
        std::pair<unsigned, char *> result = i->second;
        mTable.erase(i);
        return result;
    }

    std::pair<unsigned, char *> find(uint64_t addr) const {
        std::lock_guard<std::mutex> lock(mMutex);
        std::map<AddressRange, std::pair<unsigned, char *>>::const_iterator i = mTable.find(AddressRange(addr));
        if (i == mTable.end())
            return mNullValue;
        return i->second;
    }
}; /* RangeTable */

const unsigned SHIM_USER_BAR = 0x0;
const unsigned SHIM_MGMT_BAR = 0x10000;
const uint64_t mNullAddr = 0xffffffffffffffffull;
const uint64_t mNullBO = 0xffffffff;

class XOCLShim
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
    ~XOCLShim();
    XOCLShim(unsigned index, const char *logfileName, xclVerbosityLevel verbosity);
    void init(unsigned index, const char *logfileName, xclVerbosityLevel verbosity);
    void readDebugIpLayout();
    int xclLogMsg(xclDeviceHandle handle, xclLogMsgLevel level, const char* format, ...);
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
    int xclGetErrorStatus(xclErrorStatus *info);
    int xclGetDeviceInfo2(xclDeviceInfo2 *info);
    bool isGood() const;
    static XOCLShim *handleCheck(void * handle);
    int resetDevice(xclResetKind kind);
    int p2pEnable(bool enable, bool force);
    bool xclLockDevice();
    bool xclUnlockDevice();
    int xclReClock2(unsigned short region, const unsigned short *targetFreqMHz);
    int xclGetUsageInfo(xclDeviceUsage *info);

    int xclTestXSpi(int device_index);
    int xclBootFPGA();
    int xclRemoveAndScanFPGA();

    // Legacy buffer management API support
    uint64_t xclAllocDeviceBuffer(size_t size);
    uint64_t xclAllocDeviceBuffer2(size_t size, xclMemoryDomains domain, unsigned flags);
    void xclFreeDeviceBuffer(uint64_t buf);
    size_t xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek);
    size_t xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip);

    ssize_t xclUnmgdPwrite(unsigned flags, const void *buf, size_t count, uint64_t offset);
    ssize_t xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset);

    int xclGetSectionInfo(void *section_info, size_t *section_size, enum axlf_section_kind, int index);


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
    // Counters
    size_t xclPerfMonStartCounters(xclPerfMonType type);
    size_t xclPerfMonStopCounters(xclPerfMonType type);
    size_t xclPerfMonReadCounters(xclPerfMonType type, xclCounterResults& counterResults);

    //debug related
    uint32_t getCheckerNumberSlots(int type);
    uint32_t getIPCountAddrNames(int type, uint64_t *baseAddress, std::string * portNames,
                                    uint8_t *properties, size_t size);
    size_t xclDebugReadCounters(xclDebugCountersResults* debugResult);
    size_t xclDebugReadCheckers(xclDebugCheckersResults* checkerResult);
    size_t xclDebugReadStreamingCounters(xclStreamingDebugCountersResults* streamingResult);

    // Trace
    size_t xclPerfMonStartTrace(xclPerfMonType type, uint32_t startTrigger);
    size_t xclPerfMonStopTrace(xclPerfMonType type);
    uint32_t xclPerfMonGetTraceCount(xclPerfMonType type);
    size_t xclPerfMonReadTrace(xclPerfMonType type, xclTraceResultsVector& traceVector);

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
    xclDeviceHandle xclOpenMgmt(unsigned deviceIndex);

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
    RangeTable mLegacyAddressTable;
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

    int xclLoadAxlf(const axlf *buffer);
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

    // QDMA AIO
    aio_context_t mAioContext;
    bool mAioEnabled;
}; /* XOCLShim */

} /* xocl */

#endif
