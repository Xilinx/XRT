/**
 * Copyright (C) 2016-2019 Xilinx, Inc
 * Author(s): Jason Villarreal
 *          : Paul Schumacher
 * ZNYQ HAL Driver profiling functionality 
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

#ifndef _ZYNQ_SHIM_PROFILE_H_
#define _ZYNQ_SHIM_PROFILE_H_

#include <string>
#include "driver/include/xclperf.h"

namespace ZYNQ {
  
  class ZYNQShim; 

  // Until XDP refactoring is finished, this class handles
  //  all of the profiling functionality needed in the SHIM.
  //  This has the same lifetime as the shim object and should
  //  only be instantiated inside the shim object itself.
  class ZYNQShimProfiling 
  {
  private:
    ZYNQShim* shim ;

    // The number of each type of IP we can get information from
    uint32_t mMemoryProfilingNumberSlots;
    uint32_t mAccelProfilingNumberSlots;
    uint32_t mStallProfilingNumberSlots;
    uint32_t mStreamProfilingNumberSlots;

    bool mIsDebugIpLayoutRead = false;
    bool mIsDeviceProfiling   = false;

    // Addresses for IP that we get from debug IP layout
    uint64_t mPerfMonFifoCtrlBaseAddress = 0;
    uint64_t mPerfMonFifoReadBaseAddress = 0;
    uint64_t mTraceFunnelAddress         = 0;
    uint64_t mPerfMonBaseAddress   [XSPM_MAX_NUMBER_SLOTS]  = {};
    uint64_t mAccelMonBaseAddress  [XSAM_MAX_NUMBER_SLOTS]  = {};
    uint64_t mStreamMonBaseAddress [XSSPM_MAX_NUMBER_SLOTS] = {};

    // The names of the IP that we get from debug IP layout
    std::string mPerfMonSlotName   [XSPM_MAX_NUMBER_SLOTS]  = {};
    std::string mAccelMonSlotName  [XSAM_MAX_NUMBER_SLOTS]  = {};
    std::string mStreamMonSlotName [XSSPM_MAX_NUMBER_SLOTS] = {};

    // The properties and versions of the IP we get from debug IP layout
    uint8_t mPerfmonProperties     [XSPM_MAX_NUMBER_SLOTS]  = {};
    uint8_t mAccelmonProperties    [XSAM_MAX_NUMBER_SLOTS]  = {};
    uint8_t mStreammonProperties   [XSSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mPerfmonMajorVersions  [XSPM_MAX_NUMBER_SLOTS]  = {};
    uint8_t mAccelmonMajorVersions [XSAM_MAX_NUMBER_SLOTS]  = {};
    uint8_t mStreammonMajorVersions[XSSPM_MAX_NUMBER_SLOTS] = {};
    uint8_t mPerfmonMinorVersions  [XSPM_MAX_NUMBER_SLOTS]  = {};
    uint8_t mAccelmonMinorVersions [XSAM_MAX_NUMBER_SLOTS]  = {};
    uint8_t mStreammonMinorVersions[XSSPM_MAX_NUMBER_SLOTS] = {};

  public:
    ZYNQShimProfiling() = delete ; // Requires a shim object
    ZYNQShimProfiling(ZYNQShim* s) ;
    ~ZYNQShimProfiling() ;

    // Control 
    double xclGetDeviceClockFreqMHz();
    uint32_t getProfilingNumberSlots(xclPerfMonType type);
    void getProfilingSlotName(xclPerfMonType type, uint32_t slotnum, char* slotName, uint32_t length);

    // Counters
    size_t xclPerfMonStartCounters(xclPerfMonType type);
    size_t xclPerfMonStopCounters(xclPerfMonType type);
    size_t xclPerfMonReadCounters(xclPerfMonType type, xclCounterResults& counterResults);
    // Trace
    size_t xclPerfMonStartTrace(xclPerfMonType type, uint32_t startTrigger);
    size_t xclPerfMonStopTrace(xclPerfMonType type);
    uint32_t xclPerfMonGetTraceCount(xclPerfMonType type);
    size_t xclPerfMonReadTrace(xclPerfMonType type, xclTraceResultsVector& traceVector);
    uint64_t getTraceFunnelAddress(xclPerfMonType type) ;
    size_t resetFifos(xclPerfMonType type);
    uint64_t getPerfMonFifoBaseAddress(xclPerfMonType type, uint32_t fifonum);
    uint64_t getPerfMonFifoReadBaseAddress(xclPerfMonType type, uint32_t fifonum);
    uint32_t getPerfMonNumberSamples(xclPerfMonType type) ;
    uint64_t getHostTraceTimeNsec() ;

    // Debug related  
    uint32_t getIPCountAddrNames(int type, uint64_t *baseAddress, std::string * portNames, uint8_t *properties, uint8_t *majorVersions, uint8_t *minorVersions, size_t size) ;

    // Performance monitoring helper functions  
    void readDebugIpLayout() ;
    uint64_t getPerfMonBaseAddress(xclPerfMonType type, uint32_t slotNum);
  } ;


} ;

#endif
