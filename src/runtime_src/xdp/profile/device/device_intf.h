#ifndef _XDP_DEVICE_INTF_H_
#define _XDP_DEVICE_INTF_H_

/**
 * Copyright (C) 2016-2020 Xilinx, Inc

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

#include <fstream>
#include <list>
#include <map>
#include <cassert>
#include <vector>

namespace xdp {

// Helper methods

XDP_EXPORT
uint32_t GetDeviceTraceBufferSize(uint32_t property);

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
    uint32_t getMonitorProperties(xclPerfMonType type, uint32_t index);
    XDP_EXPORT
    void getMonitorName(xclPerfMonType type, uint32_t index, char* name, uint32_t length);
    XDP_EXPORT
    std::string getMonitorName(xclPerfMonType type, uint32_t index);
    XDP_EXPORT
    std::string getTraceMonName(xclPerfMonType type, uint32_t index);
    XDP_EXPORT
    uint32_t getTraceMonProperty(xclPerfMonType type, uint32_t index);

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
    size_t readTrace(xclTraceResultsVector& traceVector);

    /** Trace S2MM Management
     */
    bool hasTs2mm() {
      return (mPlTraceDma != nullptr);
    };
    size_t getNumberTS2MM() {
      return (mPlTraceDma != nullptr) ? 1 : 0;
    };

    XDP_EXPORT
    void resetTS2MM();
    TraceS2MM* getTs2mm() {return mPlTraceDma;};
    XDP_EXPORT
    void initTS2MM(uint64_t bufferSz, uint64_t bufferAddr, bool circular); 

    XDP_EXPORT
    uint64_t getWordCountTs2mm();
    XDP_EXPORT
    uint8_t  getTS2MmMemIndex();
    XDP_EXPORT
    void parseTraceData(void* traceData, uint64_t bytes, xclTraceResultsVector& traceVector);

    XDP_EXPORT
    void resetAIETs2mm(uint64_t index);
    XDP_EXPORT
    void initAIETs2mm(uint64_t bufferSz, uint64_t bufferAddr, uint64_t index);

    XDP_EXPORT
    uint64_t getWordCountAIETs2mm(uint64_t index);
    XDP_EXPORT
    uint8_t  getAIETs2mmMemIndex(uint64_t index);
    
    double getMaxBwRead() const {return mMaxReadBW;}
    double getMaxBwWrite() const {return mMaxWriteBW;}
    XDP_EXPORT
    void setMaxBwRead();
    XDP_EXPORT
    void setMaxBwWrite();

    inline xdp::Device* getAbstractDevice() {return mDevice;}

  private:
    // Turn on/off debug messages to stdout
    bool mVerbose = false;
    // Turns on/off all profiling functions in this class
    bool mIsDeviceProfiling = true;
    // Debug IP Layout has been read or not
    bool mIsDebugIPlayoutRead = false;

    // Depending on OpenCL or HAL flow, "mDevice" is populated with xrt_xocl::device handle or HAL handle
    xdp::Device* mDevice = nullptr;

    std::vector<AIM*> mAimList;
    std::vector<AM*>  mAmList;
    std::vector<ASM*> mAsmList;
    std::vector<NOC*> nocList;

    TraceFifoLite* mFifoCtrl    = nullptr;
    TraceFifoFull* mFifoRead    = nullptr;
    TraceFunnel*   mTraceFunnel = nullptr;

    TraceS2MM*     mPlTraceDma  = nullptr;
    std::vector<TraceS2MM*> mAieTraceDmaList;

    /*
     * Set bandwidth number to a reasonable default
     * For PCIE Device:
     *   bw_per_lane = 985 MB/s (Wikipedia on PCIE 3.0)
     *   num_lanes = 16/8/4 depending on host system
     *   total bw = bw_per_lane * num_lanes
     * For Edge Device:
     *  total bw = DDR4 memory bandwidth
     */
    double mMaxReadBW  = 9600.0;
    double mMaxWriteBW = 9600.0;

}; /* DeviceIntf */

} /* xdp */

#endif
