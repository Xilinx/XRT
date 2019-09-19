#ifndef _XDP_DEVICE_INTF_H_
#define _XDP_DEVICE_INTF_H_

/**
 * Copyright (C) 2016-2018 Xilinx, Inc

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

#include "profile_ip_access.h"
#include "aim.h"
#include "am.h"
#include "asm.h"
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

  class DeviceIntf {
  public:
    DeviceIntf() {}
    ~DeviceIntf();

  public:


#if 0
    // Raw read/write
    size_t write(uint64_t offset, const void *hostBuf, size_t size);
    size_t read(uint64_t offset, void *hostBuf, size_t size);
    size_t traceRead(void *buffer, size_t size, uint64_t addr);
#endif


    // Set device handle
    // NOTE: this is used by write, read, & traceRead
    void setDevice(xdp::Device* );

    // Debug IP layout
    void     readDebugIPlayout();
    uint32_t getNumMonitors(xclPerfMonType type);
    uint32_t getMonitorProperties(xclPerfMonType type, uint32_t index);
    void     getMonitorName(xclPerfMonType type, uint32_t index, char* name, uint32_t length);
    std::string  getMonitorName(xclPerfMonType type, uint32_t index);

    bool     isHostAIM(uint32_t index) {
       return aimList[index]->isHostMonitor();
    }
    
    // Counters
    size_t startCounters(xclPerfMonType type);
    size_t stopCounters(xclPerfMonType type);
    size_t readCounters(xclPerfMonType type, xclCounterResults& counterResults);

    // Accelerator Monitor
    void configureDataflow(bool* ipConfig);
    void configAmContext(const std::string& ctx_info);

    // Trace FIFO Management
    bool hasFIFO() {return (fifoCtrl != nullptr);};
    uint32_t getTraceCount(xclPerfMonType type);
    size_t startTrace(xclPerfMonType type, uint32_t startTrigger);
    size_t stopTrace(xclPerfMonType type);
    size_t readTrace(xclPerfMonType type, xclTraceResultsVector& traceVector);

    /** Trace S2MM Management
     */
    bool hasTs2mm() {return (traceDMA != nullptr);};
    void initTS2MM(uint64_t bufferSz, uint64_t bufferAddr);
    void resetTS2MM();
    uint8_t  getTS2MmMemIndex();
    uint64_t getWordCountTs2mm();

    void parseTraceData(void* traceData, uint64_t bytes, xclTraceResultsVector& traceVector);

  private:
    // Turn on/off debug messages to stdout
    bool mVerbose = false;
    // Turns on/off all profiling functions in this class
    bool mIsDeviceProfiling = true;
    // Debug IP Layout has been read or not
    bool mIsDebugIPlayoutRead = false;

    // Depending on OpenCL or HAL flow, "mDevice" is populated with xrt::device handle or HAL handle
    xdp::Device* mDevice = nullptr;

    std::vector<AIM*> aimList;
    std::vector<AM*>  amList;
    std::vector<ASM*> asmList;

    TraceFifoLite* fifoCtrl  = nullptr;
    TraceFifoFull* fifoRead  = nullptr;
    TraceFunnel*   traceFunnel = nullptr;

    TraceS2MM* traceDMA = nullptr;

// lapc ip data holder
//

}; /* DeviceIntf */

} /* xdp */

#endif
