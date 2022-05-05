#ifndef _XDP_DEVICE_INTF_H_
#define _XDP_DEVICE_INTF_H_

/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
 * Author(s): Paul Schumacher
 *          : Anurag Dubey
 *          : Tianhao Zhou
 * XDP device interface to HAL driver
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

#include "xclhal2.h"

#include "xdp/config.h"

#include "profile_ip_access.h"
#include "aim.h"
#include "am.h"
#include "asm.h"
#include "noc.h"
#include "traceFifoLite.h"
#include "traceFifoFull.h"
#include "traceFunnel.h"
#include "traceS2MM.h"
#include "add.h"

#include <fstream>
#include <list>
#include <map>
#include <cassert>
#include <vector>
#include <mutex>

namespace xdp {

// Helper methods

XDP_EXPORT
uint64_t GetTS2MMBufSize(bool isAIETrace = false);


class DeviceIntf {
  public:

    DeviceIntf() {}

    XDP_EXPORT
    ~DeviceIntf();

  public:
    // Set device handle
    // NOTE: this is used by write, read, & traceRead
    XDP_EXPORT
    void setDevice(xdp::Device* );

    // Debug IP layout
    XDP_EXPORT
    void readDebugIPlayout();

    XDP_EXPORT
    uint32_t getNumMonitors(xclPerfMonType type);
    XDP_EXPORT
    std::string getMonitorName(xclPerfMonType type, uint32_t index);
    XDP_EXPORT
    uint64_t getFifoSize();

    bool isHostAIM(uint32_t index) {
      return mAimList[index]->isHostMonitor();
    }
    
    // Counters
    XDP_EXPORT
    size_t startCounters();
    XDP_EXPORT
    size_t stopCounters();
    XDP_EXPORT
    size_t readCounters(xclCounterResults& counterResults);

    // Accelerator Monitor
    XDP_EXPORT
    void configureDataflow(bool* ipConfig);
    XDP_EXPORT
    void configureFa(bool* ipConfig);
    XDP_EXPORT
    void configAmContext(const std::string& ctx_info);

    // Underlying Device APIs
    XDP_EXPORT
    size_t allocTraceBuf(uint64_t sz ,uint8_t memIdx);
    XDP_EXPORT
    void freeTraceBuf(size_t bufHandle);
    XDP_EXPORT
    void* syncTraceBuf(size_t bufHandle ,uint64_t offset, uint64_t bytes);
    XDP_EXPORT
    uint64_t getDeviceAddr(size_t bufHandle);
    XDP_EXPORT
    uint64_t getAlignedTraceBufferSize(uint64_t total_bytes, unsigned int num_chunks);

    // Trace FIFO Management
    bool hasFIFO() {return (mFifoCtrl != nullptr);};
    XDP_EXPORT
    uint32_t getTraceCount();
    XDP_EXPORT
    size_t startTrace(uint32_t startTrigger);
    XDP_EXPORT
    void clockTraining(bool force = true);
    XDP_EXPORT
    size_t stopTrace();
    XDP_EXPORT
    size_t readTrace(uint32_t*& traceData) ;

    /** Trace S2MM Management
     */
    bool hasTs2mm() {
      return (!mPlTraceDmaList.empty());
    };
    size_t getNumberTS2MM() {
      return mPlTraceDmaList.size();
    };
    bool supportsCircBuf() {
      return ((1 == getNumberTS2MM()) ? (mPlTraceDmaList[0]->supportsCircBuf()) : false);
    }

    XDP_EXPORT
    void resetTS2MM(uint64_t index);
    XDP_EXPORT
    void initTS2MM(uint64_t index, uint64_t bufferSz, uint64_t bufferAddr, bool circular); 

    XDP_EXPORT
    uint64_t getWordCountTs2mm(uint64_t index, bool final);
    XDP_EXPORT
    uint8_t  getTS2MmMemIndex(uint64_t index);
    XDP_EXPORT
    void parseTraceData(uint64_t index, void* traceData, uint64_t bytes, std::vector<xclTraceResults>& traceVector);

    XDP_EXPORT
    void resetAIETs2mm(uint64_t index);
    XDP_EXPORT
    void initAIETs2mm(uint64_t bufferSz, uint64_t bufferAddr, uint64_t index);

    XDP_EXPORT
    uint64_t getWordCountAIETs2mm(uint64_t index, bool final);
    XDP_EXPORT
    uint8_t  getAIETs2mmMemIndex(uint64_t index);
    
    double getHostMaxBwRead() const {return mHostMaxReadBW;}
    double getHostMaxBwWrite() const {return mHostMaxWriteBW;}
    double getKernelMaxBwRead() const {return mKernelMaxReadBW;}
    double getKernelMaxBwWrite() const {return mKernelMaxWriteBW;}
    XDP_EXPORT
    void setHostMaxBwRead();
    XDP_EXPORT
    void setHostMaxBwWrite();
    XDP_EXPORT
    void setKernelMaxBwRead();
    XDP_EXPORT
    void setKernelMaxBwWrite();

    XDP_EXPORT
    uint32_t getDeadlockStatus();

    inline xdp::Device* getAbstractDevice() {return mDevice;}

    bool hasDeadlockDetector() {return mDeadlockDetector != nullptr;}

  private:
    // Turn on/off debug messages to stdout
    bool mVerbose = false;
    // Turns on/off all profiling functions in this class
    bool mIsDeviceProfiling = true;
    // Debug IP Layout has been read or not
    bool mIsDebugIPlayoutRead = false;

    std::mutex traceLock ;

    // Depending on OpenCL or HAL flow, "mDevice" is populated with xrt_xocl::device handle or HAL handle
    xdp::Device* mDevice = nullptr;

    std::vector<AIM*> mAimList;
    std::vector<AM*>  mAmList;
    std::vector<ASM*> mAsmList;
    std::vector<NOC*> nocList;

    TraceFifoLite* mFifoCtrl    = nullptr;
    TraceFifoFull* mFifoRead    = nullptr;

    std::vector<TraceFunnel*> mTraceFunnelList;

    std::vector<TraceS2MM*> mPlTraceDmaList;
    std::vector<TraceS2MM*> mAieTraceDmaList;
    DeadlockDetector*     mDeadlockDetector  = nullptr;

    /*
     * Set max bandwidths to reasonable defaults
     * For PCIE Device:
     *   configuration: gen 3x16, gen 4x8 
     *   encoding: 128b/130b
     * For Edge Device:
     *  total BW: DDR4 memory bandwidth
     */
    double mHostMaxReadBW    = 15753.85;
    double mHostMaxWriteBW   = 15753.85;
    double mKernelMaxReadBW  = 19250.00;
    double mKernelMaxWriteBW = 19250.00;

}; /* DeviceIntf */

} /* xdp */

#endif
