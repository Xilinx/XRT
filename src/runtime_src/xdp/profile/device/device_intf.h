#ifndef _XDP_DEVICE_INTF_H_
#define _XDP_DEVICE_INTF_H_

/**
 * Copyright (C) 2016-2022 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include <cassert>
#include <fstream>
#include <list>
#include <map>
#include <mutex>
#include <vector>

#include "core/include/xclhal2.h"
#include "core/include/xdp/common.h"
#include "core/include/xdp/trace.h"

#include "xdp/config.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/device/add.h"
#include "xdp/profile/device/aim.h"
#include "xdp/profile/device/am.h"
#include "xdp/profile/device/asm.h"
#include "xdp/profile/device/noc.h"
#include "xdp/profile/device/profile_ip_access.h"
#include "xdp/profile/device/traceFifoFull.h"
#include "xdp/profile/device/traceFifoLite.h"
#include "xdp/profile/device/traceFunnel.h"
#include "xdp/profile/device/traceS2MM.h"
#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

// Helper methods

XDP_CORE_EXPORT
uint64_t GetTS2MMBufSize(bool isAIETrace = false);


class DeviceIntf {
  public:

    DeviceIntf() {}

    XDP_CORE_EXPORT
    ~DeviceIntf();

  public:
    // Set device handle
    // NOTE: this is used by write, read, & traceRead
    XDP_CORE_EXPORT
    void setDevice(xdp::Device* );

    // Debug IP layout
    XDP_CORE_EXPORT
    void readDebugIPlayout();

    XDP_CORE_EXPORT
    uint32_t getNumMonitors(xdp::MonitorType type);
    XDP_CORE_EXPORT
    std::string getMonitorName(xdp::MonitorType type, uint32_t index);
    XDP_CORE_EXPORT
    uint64_t getFifoSize();

    // Axi Interface Monitor
    bool isHostAIM(uint32_t index) {
      return mAimList[index]->isHostMonitor();
    }
    // Turn off coarse mode if any of the kernel AIMs can't support it
    bool supportsCoarseModeAIM() {
      for (auto mon : mAimList) {
        if (!mon->isHostMonitor() && !mon->hasCoarseMode()  )
          return false;
      }
      return true;
    }

    // Counters
    XDP_CORE_EXPORT
    size_t startCounters();
    XDP_CORE_EXPORT
    size_t stopCounters();
    XDP_CORE_EXPORT
    size_t readCounters(xdp::CounterResults& counterResults);

    // Accelerator Monitor
    XDP_CORE_EXPORT
    void configureDataflow(bool* ipConfig);
    XDP_CORE_EXPORT
    void configureFa(bool* ipConfig);
    XDP_CORE_EXPORT
    void configAmContext(const std::string& ctx_info);

    // Underlying Device APIs
    XDP_CORE_EXPORT
    size_t allocTraceBuf(uint64_t sz ,uint8_t memIdx);
    XDP_CORE_EXPORT
    void freeTraceBuf(size_t id);
    XDP_CORE_EXPORT
    void* syncTraceBuf(size_t id ,uint64_t offset, uint64_t bytes);
    XDP_CORE_EXPORT
    xclBufferExportHandle exportTraceBuf(size_t id);
    XDP_CORE_EXPORT
    uint64_t getTraceBufDeviceAddr(size_t id);
    XDP_CORE_EXPORT
    uint64_t getAlignedTraceBufSize(uint64_t total_bytes, unsigned int num_chunks);

    // Trace FIFO Management
    bool hasFIFO() {return (mFifoCtrl != nullptr);};
    XDP_CORE_EXPORT
    uint32_t getTraceCount();
    XDP_CORE_EXPORT
    size_t startTrace(uint32_t startTrigger);
    XDP_CORE_EXPORT
    void clockTraining(bool force = true);
    XDP_CORE_EXPORT
    size_t stopTrace();
    XDP_CORE_EXPORT
    size_t readTrace(uint32_t*& traceData) ;

    /** Trace S2MM Management
     */
    bool hasTs2mm() {
      return (!mPlTraceDmaList.empty());
    };
    size_t getNumberTS2MM() {
      return mPlTraceDmaList.size();
    };

    // All datamovers support circular buffer for PL Trace
    bool supportsCircBufPL() {
      if (mPlTraceDmaList.size() > 0)
        return mPlTraceDmaList[0]->supportsCircBuf();
      return false;
    }

    // Only version 2 Datamover supports circular buffer/flush for AIE Trace
    bool supportsCircBufAIE() {
      if (mAieTraceDmaList.size() > 0)
        return mAieTraceDmaList[0]->isVersion2();
      return false;
    }
    bool supportsflushAIE() {
      if (mAieTraceDmaList.size() > 0)
        return mAieTraceDmaList[0]->isVersion2();
      return false;
    }

    XDP_CORE_EXPORT
    void resetTS2MM(uint64_t index);
    XDP_CORE_EXPORT
    void initTS2MM(uint64_t index, uint64_t bufferSz, uint64_t bufferAddr, bool circular); 

    XDP_CORE_EXPORT
    uint64_t getWordCountTs2mm(uint64_t index, bool final);
    XDP_CORE_EXPORT
    uint8_t  getTS2MmMemIndex(uint64_t index);
    XDP_CORE_EXPORT
      void parseTraceData(uint64_t index, void* traceData, uint64_t bytes, std::vector<xdp::TraceEvent>& traceVector);

    XDP_CORE_EXPORT
    void resetAIETs2mm(uint64_t index);
    XDP_CORE_EXPORT
    void initAIETs2mm(uint64_t bufferSz, uint64_t bufferAddr, uint64_t index, bool circular);

    XDP_CORE_EXPORT
    uint64_t getWordCountAIETs2mm(uint64_t index, bool final);
    XDP_CORE_EXPORT
    uint8_t  getAIETs2mmMemIndex(uint64_t index);
    
    double getHostMaxBwRead() const {return mHostMaxReadBW;}
    double getHostMaxBwWrite() const {return mHostMaxWriteBW;}
    double getKernelMaxBwRead() const {return mKernelMaxReadBW;}
    double getKernelMaxBwWrite() const {return mKernelMaxWriteBW;}
    XDP_CORE_EXPORT
    void setHostMaxBwRead();
    XDP_CORE_EXPORT
    void setHostMaxBwWrite();
    XDP_CORE_EXPORT
    void setKernelMaxBwRead();
    XDP_CORE_EXPORT
    void setKernelMaxBwWrite();

    XDP_CORE_EXPORT
    uint32_t getDeadlockStatus();

    inline xdp::Device* getAbstractDevice() {return mDevice;}

    bool hasDeadlockDetector() {return mDeadlockDetector != nullptr;}

    bool hasHSDPforPL() { return mHSDPforPL; }

  private:
    // Turn on/off debug messages to stdout
    bool mVerbose = false;
    // Turns on/off all profiling functions in this class
    bool mIsDeviceProfiling = true;
    // Debug IP Layout has been read or not
    bool mIsDebugIPlayoutRead = false;

    // HSDP Trace IP is used for PL Trace offload
    bool mHSDPforPL = false;

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

    // Deadlock Detection and Diagnosis
    DeadlockDetector*     mDeadlockDetector  = nullptr;

    /*
     * Set max bandwidths to reasonable defaults
     * For PCIE Device:
     *   configuration: gen 3x16, gen 4x8 
     *   encoding: 128b/130b
     * For Edge Device:
     *  total BW: DDR4 memory bandwidth
     */
    double mHostMaxReadBW    = hw_constants::pcie_gen3x16_bandwidth;
    double mHostMaxWriteBW   = hw_constants::pcie_gen3x16_bandwidth;
    double mKernelMaxReadBW  = hw_constants::ddr4_2400_bandwidth;
    double mKernelMaxWriteBW = hw_constants::ddr4_2400_bandwidth;

}; /* DeviceIntf */

} /* xdp */

#endif
