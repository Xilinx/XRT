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
#include <fstream>
#include <list>
#include <map>
#include <cassert>
#include <vector>

namespace xdp {

  class DeviceIntf {
  public:
    DeviceIntf() {}
    ~DeviceIntf() {}

  public:
    // Raw read/write
    size_t write(uint64_t offset, const void *hostBuf, size_t size);
    size_t read(uint64_t offset, void *hostBuf, size_t size);
    size_t traceRead(void *buffer, size_t size, uint64_t addr);

    // Set device handle
    // NOTE: this is used by write, read, & traceRead
    void setDeviceHandle(xclDeviceHandle handle) {mHandle = handle;}

    // Generic helpers
    uint64_t getHostTraceTimeNsec();
    uint32_t getMaxSamples(xclPerfMonType type);
    std::string dec2bin(uint32_t n);
    std::string dec2bin(uint32_t n, unsigned bits);

    // Debug IP layout
    uint64_t getBaseAddress(xclPerfMonType type, uint32_t slotNum);
    uint64_t getFifoBaseAddress(xclPerfMonType type, uint32_t fifonum);
    uint64_t getFifoReadBaseAddress(xclPerfMonType type, uint32_t fifonum);
    uint64_t getTraceFunnelAddress(xclPerfMonType type);
    uint32_t getNumberSlots(xclPerfMonType type);
    uint32_t getProperties(xclPerfMonType type, uint32_t slotnum);
    void getSlotName(xclPerfMonType type, uint32_t slotnum,
                     char* slotName, uint32_t length);

    // Counters
    size_t startCounters(xclPerfMonType type);
    size_t stopCounters(xclPerfMonType type);
    size_t readCounters(xclPerfMonType type, xclCounterResults& counterResults);

    // Trace
    size_t resetTraceFifo(xclPerfMonType type);
    size_t clockTraining(xclPerfMonType type);
    uint32_t getTraceCount(xclPerfMonType type);
    size_t startTrace(xclPerfMonType type, uint32_t startTrigger);
    size_t stopTrace(xclPerfMonType type);
    size_t readTrace(xclPerfMonType type, xclTraceResultsVector& traceVector);

  private:
    // Turn on/off debug messages to stdout
    bool mVerbose = true;
    // Turns on/off all profiling functions in this class
    bool mIsDeviceProfiling = true;
    // Device handle - used in all HAL function calls
    xclDeviceHandle mHandle = nullptr;

    // Information extracted from debug IP layout
    uint64_t mFifoCtrlBaseAddress = 0;
    uint64_t mFifoReadBaseAddress = 0;
    uint64_t mTraceFunnelAddress = 0;
    uint32_t mMemoryProfilingNumberSlots = 0;
    uint32_t mAccelProfilingNumberSlots = 0;
    uint32_t mStallProfilingNumberSlots = 0;
    uint32_t mStreamProfilingNumberSlots = 0;
    uint64_t mBaseAddress[XSPM_MAX_NUMBER_SLOTS] = {};
    uint64_t mAccelMonBaseAddress[XSAM_MAX_NUMBER_SLOTS] = {};
    uint64_t mStreamMonBaseAddress[XSSPM_MAX_NUMBER_SLOTS] = {};
    std::string mSlotName[XSPM_MAX_NUMBER_SLOTS] = {};
    std::string mAccelMonSlotName[XSAM_MAX_NUMBER_SLOTS] = {};
    std::string mStreamMonSlotName[XSSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mProperties[XSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mAccelmonProperties[XSAM_MAX_NUMBER_SLOTS] = {};
    uint8_t mStreammonProperties[XSSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mMajorVersions[XSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mAccelmonMajorVersions[XSAM_MAX_NUMBER_SLOTS] = {};
    uint8_t mStreammonMajorVersions[XSSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mMinorVersions[XSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mAccelmonMinorVersions[XSAM_MAX_NUMBER_SLOTS] = {};
    uint8_t mStreammonMinorVersions[XSSPM_MAX_NUMBER_SLOTS] = {};
}; /* DeviceIntf */

} /* xdp */

#endif
