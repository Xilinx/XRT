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

#ifndef _XDMA_SHIM_H_
#define _XDMA_SHIM_H_

/**
 * Copyright (C) 2015 Xilinx, Inc
 * Author: Sonal Santan
 * CAPI HAL Driver layered on top of POWER8 libcxl
 */

#include "xclhal.h"


#include <fstream>
#include <list>
#include <string>
#include <mutex>
#include <sys/types.h>

#if defined(__GNUC__) && defined(NDEBUG)
#define SHIM_O2 __attribute__ ((optimize("-O2")))
#else
#define SHIM_O2
#endif


struct cxl_adapter_h *mAdapter;
struct cxl_afu_h *mAFU;

namespace xclcapi {
    // Memory alignment for DDR and AXI-MM trace access
    template <typename T> class AlignedAllocator {
        void *mBuffer;
        size_t mCount;
    public:
        T *getBuffer() {
            return (T *)mBuffer;
        }

        size_t size() const {
            return mCount * sizeof(T);
        }

        AlignedAllocator(size_t alignment, size_t count) : mBuffer(0), mCount(count) {
            if (posix_memalign(&mBuffer, alignment, count * sizeof(T))) {
                mBuffer = 0;
            }
        }
        ~AlignedAllocator() {
            if (mBuffer)
                free(mBuffer);
        }
    };

    // CAPI Shim
    class CAPIShim {
        typedef std::list<std::pair<uint64_t, uint64_t> > PairList;
        static const int BUFFER_ALIGNMENT = 0x80;

    public:

        // Bitstreams
        int xclLoadBitstreamIoctl(const char *fileName);

        //int xclLoadBitstream(const char *fileName);
        int xclLoadXclBin(const axlf *buffer);
        int xclUpgradeFirmware(const char *fileName);
        int xclBootFPGA();
        int resetDevice(xclResetKind kind);

        // Raw read/write
        size_t xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
        size_t xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);

        // Buffer management
        uint64_t xclAllocDeviceBuffer(size_t size);
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
        static CAPIShim *handleCheck(void *handle);
        static unsigned xclProbe();
        unsigned getTAG() const;
        bool isGood() const;
        bool isUltraScale() const {
            return mIsUltraScale;
        }

        ~CAPIShim();
        CAPIShim(unsigned index, const char *logfileName, xclVerbosityLevel verbosity);

    private:

        int initAFU();
        void exitAFU();
        size_t xclReadModifyWrite(uint64_t offset, const void *hostBuf, size_t size);
        size_t xclReadSkipCopy(uint64_t offset, void *hostBuf, size_t size);
        int xclLoadBitstreamWorker(std::FILE *bit_file);
        // Core DMA code
        SHIM_O2 int pcieBarRead(unsigned long long offset, void* buffer, unsigned long long length);
        SHIM_O2 int pcieBarWrite(unsigned long long offset, const void* buffer, unsigned long long length);
        int xclLoadBitstreamWorker(const char *fileName);

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

    private:
        // This is a hidden signature of this class and helps in preventing
        // user errors when incorrect pointers are passed in as handles.
        const unsigned mTag;
        const int mBoardNumber;
        bool mHasSysDriver;
        bool mIsUltraScale;

#ifndef _WINDOWS
// TODO: Windows build support
        // mOffsets doesn't seem to be used
        // and it caused window compilation error when we try to initialize it
        const uint64_t mOffsets[XCL_ADDR_SPACE_MAX];
#endif

        cxl_adapter_h *mAdapter;
        cxl_afu_h *mAFU;
        uint64_t *mWed;
        unsigned long mMmioSize;
        int mH2DHandle;
        int mD2HHandle;
        int mControlHandle;
        int mUserHandle;
        int mDSAMajorVersion;
        int mDSAMinorVersion;
        uint32_t mOclRegionProfilingNumberSlots;

        char *mControlMap;
        char *mUserMap;
        std::mutex mMemManagerMutex;
        std::list<std::pair<uint64_t, uint64_t> > mBusyBufferList;
        std::ofstream mLogStream;
        xclVerbosityLevel mVerbosity;
        std::string mBinfile;

    public:
        static const unsigned TAG;
    };
}

#endif


