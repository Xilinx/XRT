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
    void setDeviceHandle(void* xrtDevice);

    // Debug IP layout
    void     readDebugIPlayout();
    uint32_t getNumMonitors(xclPerfMonType type);
    uint32_t getMonitorProperties(xclPerfMonType type, uint32_t index);
    void     getMonitorName(xclPerfMonType type, uint32_t index, char* name, uint32_t length);
    
    // Counters
    size_t startCounters(xclPerfMonType type);
    size_t stopCounters(xclPerfMonType type);
    size_t readCounters(xclPerfMonType type, xclCounterResults& counterResults);

    // Enable Dataflow
    void configureDataflow(bool* ipConfig);

    // Trace FIFO Management
    bool hasFIFO() {return (fifoCtrl != nullptr);};
    uint32_t getTraceCount(xclPerfMonType type);
    size_t startTrace(xclPerfMonType type, uint32_t startTrigger);
    size_t stopTrace(xclPerfMonType type);
    size_t readTrace(xclPerfMonType type, xclTraceResultsVector& traceVector);
    /** Trace S2MM Management
     * The BO is managed internal to device
     */
    bool hasTs2mm() {return (traceDMA != nullptr);};
    bool initTs2mm(uint64_t bo_size);
    /** 
     * Takes the offset inside the mapped buffer
     * and syncs it with device and returns its virtual address.
     * We can read the entire buffer in one go if we want to
     * or choose to read in chunks
     */
    uint64_t getWordCountTs2mm();
    void* syncTraceBO(uint64_t offset, uint64_t bytes);
    void readTs2mm(uint64_t offset, uint64_t bytes, xclTraceResultsVector& traceVector);
    /**
     * This reader needs to be initialized once and then
     * returns data as long as it's available
     * returns true if data equal to chunksize was read
     */
    bool readTs2mm(xclTraceResultsVector& traceVector);
    void configReaderTs2mm(uint64_t chunksize);
    void finTs2mm();

  private:
    // Turn on/off debug messages to stdout
    bool mVerbose = false;
    // Turns on/off all profiling functions in this class
    bool mIsDeviceProfiling = true;
    // Debug IP Layout has been read or not
    bool mIsDebugIPlayoutRead = false;
    // Device handle - xrt::device handle
    void* mDeviceHandle = nullptr;

    uint64_t mBytesTs2mm = 0;
    uint64_t mChunksizeTs2mm = 0;
    uint64_t mOffsetTs2mm = 0;

    uint64_t mTs2mmBoSize = 0;
    xrt::hal::BufferObjectHandle mTs2mmBoHandle = nullptr;

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
