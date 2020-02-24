#ifndef _XOCL_GEM_SHIM_H_
#define _XOCL_GEM_SHIM_H_

/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Author(s): Umang Parekh
 *          : Sonal Santan
 *          : Ryan Radjabi
 *
 * XRT PCIe library layered on top of xocl kernel driver
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

#include "scan.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "xclhal2.h"
#include "core/pcie/driver/linux/include/xocl_ioctl.h"
#include "core/pcie/driver/linux/include/qdma_ioctl.h"
#include "core/common/xrt_profiling.h"

#include <linux/aio_abi.h>
#include <libdrm/drm.h>

#include <mutex>
#include <fstream>
#include <list>
#include <map>
#include <cassert>
#include <vector>
#include <memory>

// Forward declaration
namespace xrt_core {
    class bo_cache;
}

namespace xocl {

const uint64_t mNullAddr = 0xffffffffffffffffull;
const uint64_t mNullBO = 0xffffffff;

class shim
{
public:
    ~shim();
    shim(unsigned index, const char *logfileName, xclVerbosityLevel verbosity);
    void init(unsigned index, const char *logfileName, xclVerbosityLevel verbosity);

    static int xclLogMsg(xrtLogMsgLevel level, const char* tag, const char* format, va_list args1);
    // Raw unmanaged read/write on the entire PCIE user BAR
    size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
    size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
    // Restricted read/write on IP register space
    int xclRegWrite(uint32_t ipIndex, uint32_t offset, uint32_t data);
    int xclRegRead(uint32_t ipIndex, uint32_t offset, uint32_t *datap);

    unsigned int xclAllocBO(size_t size, int unused, unsigned flags);
    unsigned int xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags);
    void xclFreeBO(unsigned int boHandle);
    int xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek);
    int xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip);
    void *xclMapBO(unsigned int boHandle, bool write);
    int xclUnmapBO(unsigned int boHandle, void* addr);
    int xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset);
    int xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size,
                  size_t dst_offset, size_t src_offset);

    int xclUpdateSchedulerStat();

    int xclExportBO(unsigned int boHandle);
    unsigned int xclImportBO(int fd, unsigned flags);
    int xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties);

    // Bitstream/bin download
    int xclLoadXclBin(const xclBin *buffer);
    int xclGetErrorStatus(xclErrorStatus *info);
    int xclGetDeviceInfo2(xclDeviceInfo2 *info);
    bool isGood() const;
    static shim *handleCheck(void * handle);
    int resetDevice(xclResetKind kind);
    int p2pEnable(bool enable, bool force);
    int cmaEnable(bool enable, uint64_t size);
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

    double xclGetDeviceClockFreqMHz();
    double xclGetReadMaxBandwidthMBps();
    double xclGetWriteMaxBandwidthMBps();
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

    // APIs using sysfs information
    uint32_t xclGetNumLiveProcesses();
    int xclGetSysfsPath(const char* subdev, const char* entry, char* sysfsPath, size_t size);

    /* Enable/disable CMA chunk with specific size
     * e.g. enable = true, sz = 0x100000 (2M): add 2M CMA chunk
     *      enable = false: remove CMA chunk
     */
    int xclCmaEnable(xclDeviceHandle handle, bool enable, uint64_t sz);

    int xclGetDebugIPlayoutPath(char* layoutPath, size_t size);
    int xclGetSubdevPath(const char* subdev, uint32_t idx, char* path, size_t size);
    int xclGetTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz);
    int xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample);

    // Experimental debug profile device data API
    int xclGetDebugProfileDeviceInfo(xclDebugProfileDeviceInfo* info);

    // Execute and interrupt abstraction
    int xclExecBuf(unsigned int cmdBO);
    int xclExecBuf(unsigned int cmdBO,size_t numdeps, unsigned int* bo_wait_list);
    int xclRegisterEventNotify(unsigned int userInterrupt, int fd);
    int xclExecWait(int timeoutMilliSec);
    int xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared) const;
    int xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex);

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
    int xclIPName2Index(const char *name, uint32_t& index);

private:
    std::shared_ptr<xrt_core::device> mCoreDevice;
    std::shared_ptr<pcidev::pci_device> mDev;
    xclVerbosityLevel mVerbosity;
    std::ofstream mLogStream;
    int mUserHandle;
    int mStreamHandle;
    int mBoardNumber;
    bool mLocked;
    const char *mLogfileName;
    uint64_t mOffsets[XCL_ADDR_SPACE_MAX];
    xclDeviceInfo2 mDeviceInfo;
    uint32_t mMemoryProfilingNumberSlots;
    uint32_t mAccelProfilingNumberSlots;
    uint32_t mStallProfilingNumberSlots;
    uint32_t mStreamProfilingNumberSlots;
    std::string mDevUserName;
    std::unique_ptr<xrt_core::bo_cache> mCmdBOCache;

    /*
     * Mapped CU register space for xclRegRead/Write(). We support at most
     * 128 CUs and each map is of 64k bytes.
     */
    std::vector<uint32_t*> mCuMaps;
    const size_t mCuMapSize = 64 * 1024;
    std::mutex mCuMapLock;

    bool zeroOutDDR();
    bool isXPR() const {
        return ((mDeviceInfo.mSubsystemId >> 12) == 4);
    }

    int dev_init();
    void dev_fini();

    int xclLoadAxlf(const axlf *buffer);
    void xclSysfsGetDeviceInfo(xclDeviceInfo2 *info);
    void xclSysfsGetUsageInfo(drm_xocl_usage_stat& stat);
    void xclSysfsGetErrorStatus(xclErrorStatus& stat);
    int xrt_logmsg(xrtLogMsgLevel level, const char* format, ...);

    int freezeAXIGate();
    int freeAXIGate();

    int xclRegRW(bool rd, uint32_t ipIndex, uint32_t offset, uint32_t *datap);

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
    signed cmpMonVersions(unsigned major1, unsigned minor1, unsigned major2, unsigned minor2);

    // QDMA AIO
    aio_context_t mAioContext;
    bool mAioEnabled;
}; /* shim */

} /* xocl */

#endif
