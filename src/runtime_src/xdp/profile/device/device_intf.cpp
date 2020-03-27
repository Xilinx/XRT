/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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

#define XDP_SOURCE

#include "device_intf.h"
#include "xclperf.h"
#include "xcl_perfmon_parameters.h"
#include "tracedefs.h"
#include "core/common/message.h"

#include <iostream>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <regex>

#ifndef _WINDOWS
// TODO: Windows build support
//    unistd.h is linux only header file
//    it is included for read, write, close, lseek64
#include <unistd.h>
#endif

#ifdef _WINDOWS
#define __func__ __FUNCTION__
#endif

#ifdef _WIN32
#pragma warning (disable : 4996 4267)
/* 4996 : Disable warning for use of strncpy */
/* 4267 : Disable warning for conversion of size_t to uint32_t in return statements in "getNumMonitors" */
#endif

namespace xdp {

// Helper function

// Same as defined in vpl tcl
uint32_t GetDeviceTraceBufferSize(uint32_t property)
{
  switch(property) {
    case 0 : return 8192;
    case 1 : return 1024;
    case 2 : return 2048;
    case 3 : return 4096;
    case 4 : return 16384;
    case 5 : return 32768;
    case 6 : return 65536;
    case 7 : return 131072;
    default : break;
  }
  return 8192;
}


uint64_t GetTS2MMBufSize()
{
  std::string size_str = xrt_core::config::get_trace_buffer_size();
  std::smatch pieces_match;
  // Default is 1M
  uint64_t bytes = 1048576;
  // Regex can parse values like : "1024M" "1G" "8192k"
  const std::regex size_regex("\\s*([0-9]+)\\s*(K|k|M|m|G|g|)\\s*");
  if (std::regex_match(size_str, pieces_match, size_regex)) {
    try {
      if (pieces_match[2] == "K" || pieces_match[2] == "k") {
        bytes = std::stoull(pieces_match[1]) * 1024;
      } else if (pieces_match[2] == "M" || pieces_match[2] == "m") {
        bytes = std::stoull(pieces_match[1]) * 1024 * 1024;
      } else if (pieces_match[2] == "G" || pieces_match[2] == "g") {
        bytes = std::stoull(pieces_match[1]) * 1024 * 1024 * 1024;
      } else {
        bytes = std::stoull(pieces_match[1]);
      }
    } catch (const std::exception& ) {
      // User specified number cannot be parsed
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", TS2MM_WARN_MSG_BUFSIZE_DEF);
    }
  } else {
    xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", TS2MM_WARN_MSG_BUFSIZE_DEF);
  }
  if (bytes > TS2MM_MAX_BUF_SIZE) {
    bytes = TS2MM_MAX_BUF_SIZE;
    xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", TS2MM_WARN_MSG_BUFSIZE_BIG);
  }
  if (bytes < TS2MM_MIN_BUF_SIZE) {
    bytes = TS2MM_MIN_BUF_SIZE;
    xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", TS2MM_WARN_MSG_BUFSIZE_SMALL);
  }
  return bytes;
}


DeviceIntf::~DeviceIntf()
{
    for(auto mon : aimList) {
        delete mon;
    }
    for(auto mon : amList) {
        delete mon;
    }
    for(auto mon : asmList) {
        delete mon;
    }
    aimList.clear();
    amList.clear();
    asmList.clear();

    delete fifoCtrl;
    delete fifoRead;
    delete traceFunnel;
    delete traceDMA;

    delete mDevice;
}

  // ***************************************************************************
  // Read/Write
  // ***************************************************************************

#if 0
  size_t DeviceIntf::write(uint64_t offset, const void *hostBuf, size_t size)
  {
    if (mDevice == nullptr)
      return 0;
	return xclWrite(mDevice, XCL_ADDR_SPACE_DEVICE_PERFMON, offset, hostBuf, size);
  }

  size_t DeviceIntf::read(uint64_t offset, void *hostBuf, size_t size)
  {
    if (mDevice == nullptr)
      return 0;
	return xclRead(mDevice, XCL_ADDR_SPACE_DEVICE_PERFMON, offset, hostBuf, size);
  }

  size_t DeviceIntf::traceRead(void *buffer, size_t size, uint64_t addr)
  {
    if (mDevice == nullptr)
      return 0;
    return xclUnmgdPread(mDevice, 0, buffer, size, addr);
  }
#endif

  void DeviceIntf::setDevice(xdp::Device* devHandle)
  {
    if(mDevice && mDevice != devHandle) {
      // ERROR : trying to set device when it is already populated with some other device
      return;
    }
    mDevice = devHandle; 
  }

  // ***************************************************************************
  // Debug IP Layout
  // ***************************************************************************
  
  uint32_t DeviceIntf::getNumMonitors(xclPerfMonType type)
  {
    if (type == XCL_PERF_MON_MEMORY)
      return aimList.size();
    if (type == XCL_PERF_MON_ACCEL)
      return amList.size();
    if (type == XCL_PERF_MON_STR)
      return asmList.size();

    if(type == XCL_PERF_MON_STALL) {
      uint32_t count = 0;
      for(auto mon : amList) {
        if(mon->hasStall())  count++;
      }
      return count;
    }

    if(type == XCL_PERF_MON_HOST) {
      uint32_t count = 0;
      for(auto mon : aimList) {
        if(mon->isHostMonitor())  count++;
      }
      return count;
    }

    // FIFO ?

    if(type == XCL_PERF_MON_SHELL) {
      uint32_t count = 0;
      for(auto mon : aimList) {
        if(mon->isShellMonitor())  count++;
      }
      return count;
    }
    return 0;
  }

  void DeviceIntf::getMonitorName(xclPerfMonType type, uint32_t index, char* name, uint32_t length)
  {
    std::string str = "";
    if((type == XCL_PERF_MON_MEMORY) && (index < aimList.size())) { str = aimList[index]->getName(); }
    if((type == XCL_PERF_MON_ACCEL)  && (index < amList.size()))  { str = amList[index]->getName(); }
    if((type == XCL_PERF_MON_STR)    && (index < asmList.size())) { str = asmList[index]->getName(); }
    strncpy(name, str.c_str(), length);
    if(str.length() >= length) name[length-1] = '\0'; // required ??
  }

  std::string DeviceIntf::getMonitorName(xclPerfMonType type, uint32_t index)
  {
    if((type == XCL_PERF_MON_MEMORY) && (index < aimList.size())) { return aimList[index]->getName(); }
    if((type == XCL_PERF_MON_ACCEL)  && (index < amList.size()))  { return amList[index]->getName(); }
    if((type == XCL_PERF_MON_STR)    && (index < asmList.size())) { return asmList[index]->getName(); }
    return std::string("");
  }

  std::string DeviceIntf::getTraceMonName(xclPerfMonType type, uint32_t index)
  {
    if (type == XCL_PERF_MON_MEMORY) {
      for (auto& ip: aimList) {
        if (ip->hasTraceID(index))
          return ip->getName();
      }
    }
    if (type == XCL_PERF_MON_ACCEL) {
      for (auto& ip: amList) {
        if (ip->hasTraceID(index))
          return ip->getName();
      }
    }
    if (type == XCL_PERF_MON_STR) {
      for (auto& ip: asmList) {
        if (ip->hasTraceID(index))
          return ip->getName();
      }
    }
    return std::string("");
  }

  uint32_t DeviceIntf::getTraceMonProperty(xclPerfMonType type, uint32_t index)
  {
    if (type == XCL_PERF_MON_MEMORY) {
      for (auto& ip: aimList) {
        if (ip->hasTraceID(index))
          return ip->getProperties();;
      }
    }
    if (type == XCL_PERF_MON_ACCEL) {
      for (auto& ip: amList) {
        if (ip->hasTraceID(index))
          return ip->getProperties();;
      }
    }
    if (type == XCL_PERF_MON_STR) {
      for (auto& ip: asmList) {
        if (ip->hasTraceID(index))
          return ip->getProperties();;
      }
    }
    return 0;
  }

  uint32_t DeviceIntf::getMonitorProperties(xclPerfMonType type, uint32_t index)
  {
    if((type == XCL_PERF_MON_MEMORY) && (index < aimList.size())) { return aimList[index]->getProperties(); }
    if((type == XCL_PERF_MON_ACCEL)  && (index < amList.size()))  { return amList[index]->getProperties(); }
    if((type == XCL_PERF_MON_STR)    && (index < asmList.size())) { return asmList[index]->getProperties(); }
    if(type == XCL_PERF_MON_FIFO) { return fifoRead->getProperties(); }
    return 0;
  }

  // ***************************************************************************
  // Counters
  // ***************************************************************************

  // Start device counters performance monitoring
  size_t DeviceIntf::startCounters()
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
                << ", Start device counters..." << std::endl;
    }

    // Update addresses for debug/profile IP
//    readDebugIPlayout();

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;

    // Axi Interface Mons
    for(auto mon : aimList) {
        size += mon->startCounter();
    }
    // Accelerator Mons
    for(auto mon : amList) {
        size += mon->startCounter();
    }

    // Axi Stream Mons
    for(auto mon : asmList) {
        size += mon->startCounter();
    }
    return size;
  }

  // Stop both profile and trace performance monitoring
  size_t DeviceIntf::stopCounters() {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
                << ", Stop and reset device counters..." << std::endl;
    }

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;

    // Axi Interface Mons
    for(auto mon : aimList) {
        size += mon->stopCounter();
    }


#if 0
    // These aren't enabled in IP
    // Accelerator Mons
    for(auto mon : amList) {
        size += mon->stopCounter();
    }

    // Axi Stream Mons
    for(auto mon : asmList) {
        size += mon->stopCounter();
    }
#endif
    return size;
  }

  // Read AIM performance counters
  size_t DeviceIntf::readCounters(xclCounterResults& counterResults) {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
      << ", " << &counterResults
      << ", Read device counters..." << std::endl;
    }

    // Initialize all values in struct to 0
    memset(&counterResults, 0, sizeof(xclCounterResults));

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;

    // Read all Axi Interface Mons
    uint32_t idx = 0;
    for(auto mon : aimList) {
        size += mon->readCounter(counterResults, idx++);
    }

    // Read all Accelerator Mons
    idx = 0;
    for(auto mon : amList) {
        size += mon->readCounter(counterResults, idx++);
    }

    // Read all Axi Stream Mons
    idx = 0;
    for(auto mon : asmList) {
        size += mon->readCounter(counterResults, idx++);
    }

    return size;
  }

  // ***************************************************************************
  // Timeline Trace
  // ***************************************************************************

  // Start trace performance monitoring
  size_t DeviceIntf::startTrace(uint32_t startTrigger)
  {
    // StartTrigger Bits:
    // Bit 0: Trace Coarse/Fine     Bit 1: Transfer Trace Ctrl
    // Bit 2: CU Trace Ctrl         Bit 3: INT Trace Ctrl
    // Bit 4: Str Trace Ctrl        Bit 5: Ext Trace Ctrl
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
                << ", " << startTrigger
                << ", Start device tracing..." << std::endl;
    }
    size_t size = 0;

    // These should be reset before anything
    if (fifoCtrl)
      fifoCtrl->reset();
    if (traceFunnel)
      traceFunnel->reset();

    // This just writes to trace control register
    // Axi Interface Mons
    for(auto mon : aimList) {
        size += mon->triggerTrace(startTrigger);
    }
    // Accelerator Mons
    for(auto mon : amList) {
        size += mon->triggerTrace(startTrigger);
    }
    // Axi Stream Mons
    for(auto mon : asmList) {
        size += mon->triggerTrace(startTrigger);
    }

    uint32_t traceVersion = 0;
    if (traceFunnel) {
      if (traceFunnel->compareVersion(1,0) == -1)
        traceVersion = 1;
    }

    if (fifoRead)
      fifoRead->setTraceFormat(traceVersion);

    if (traceDMA)
      traceDMA->setTraceFormat(traceVersion);

    return size;
  }

  void DeviceIntf::clockTraining(bool force)
  {
    if(!traceFunnel)
      return;
    // Trace Funnel > 1.0 supports continuous training
    if (traceFunnel->compareVersion(1,0) == -1 || force == true)
      traceFunnel->initiateClockTraining();
  }

  // Stop trace performance monitoring
  size_t DeviceIntf::stopTrace()
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
                << ", Stop and reset device tracing..." << std::endl;
    }

    if (!mIsDeviceProfiling || !fifoCtrl)
   	  return 0;

    return fifoCtrl->reset();
  }

  // Get trace word count
  uint32_t DeviceIntf::getTraceCount() {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

    if (!mIsDeviceProfiling || !fifoCtrl)
   	  return 0;

    return fifoCtrl->getNumTraceSamples();
  }

  // Read all values from APM trace AXI stream FIFOs
  size_t DeviceIntf::readTrace(xclTraceResultsVector& traceVector)
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
                << ", " << &traceVector
                << ", Reading device trace stream..." << std::endl;
    }

    traceVector.mLength = 0;
    if (!mIsDeviceProfiling || !fifoRead)
   	  return 0;

    size_t size = 0;
    size += fifoRead->readTrace(traceVector, getTraceCount());

    return size;
  }

  void DeviceIntf::readDebugIPlayout()
  {
    if(mIsDebugIPlayoutRead || !mDevice)
        return;

#ifndef _WIN32
    std::string path = mDevice->getDebugIPlayoutPath();
    if(path.empty()) {
        // error ? : for HW_emu this will be empty for now ; but as of current status should not have been called 
        return;
    }

    uint32_t liveProcessesOnDevice = mDevice->getNumLiveProcesses();
    if(liveProcessesOnDevice > 1) {
      /* More than 1 process on device. Device Profiling for multi-process not supported yet.
       */
      std::string warnMsg = "Multiple live processes running on device. Hardware Debug and Profiling data will be unavailable for this process.";
      std::cout << warnMsg << std::endl;
//      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", warnMsg) ;
      mIsDeviceProfiling = false;
      mIsDebugIPlayoutRead = true;
      return;
    }

    std::ifstream ifs(path.c_str(), std::ifstream::binary);
    if(!ifs) {
      return;
    }

    char buffer[65536];
    // debug_ip_layout max size is 65536
    ifs.read(buffer, 65536);


    debug_ip_layout *map;
    if (ifs.gcount() > 0) {
      map = (debug_ip_layout*)(buffer);
#else
    size_t sz1 = 0, sectionSz = 0;
    // Get the size of full debug_ip_layout
    mDevice->getDebugIpLayout(nullptr, sz1, &sectionSz);
    // Allocate buffer to retrieve debug_ip_layout information from loaded xclbin
    std::vector<char> buffer(sectionSz);
    mDevice->getDebugIpLayout(buffer.data(), sectionSz, &sz1);
    auto map = reinterpret_cast<debug_ip_layout*>(buffer.data());
#endif
      
      for(uint64_t i = 0; i < map->m_count; i++ ) {
      switch(map->m_debug_ip_data[i].m_type) {
        case AXI_MM_MONITOR :        aimList.push_back(new AIM(mDevice, i, &(map->m_debug_ip_data[i])));
                                     break;
        case ACCEL_MONITOR  :        amList.push_back(new AM(mDevice, i, &(map->m_debug_ip_data[i])));
                                     break;
        case AXI_STREAM_MONITOR :    asmList.push_back(new ASM(mDevice, i, &(map->m_debug_ip_data[i])));
                                     break;
        case AXI_MONITOR_FIFO_LITE : fifoCtrl = new TraceFifoLite(mDevice, i, &(map->m_debug_ip_data[i]));
                                     break;
        case AXI_MONITOR_FIFO_FULL : fifoRead = new TraceFifoFull(mDevice, i, &(map->m_debug_ip_data[i]));
                                     break;
        case AXI_TRACE_FUNNEL :      traceFunnel = new TraceFunnel(mDevice, i, &(map->m_debug_ip_data[i]));
                                     break;
        case TRACE_S2MM :            traceDMA = new TraceS2MM(mDevice, i, &(map->m_debug_ip_data[i]));
                                     break;
        default : break;
        // case AXI_STREAM_PROTOCOL_CHECKER

      }
     }
#ifndef _WIN32
    }
    ifs.close();
#endif

    auto sorter = [] (const ProfileIP* lhs, const ProfileIP* rhs)
    {
      return lhs->getMIndex() < rhs->getMIndex();
    };
    std::sort(aimList.begin(), aimList.end(), sorter);
    std::sort(amList.begin(), amList.end(), sorter);
    std::sort(asmList.begin(), asmList.end(), sorter);

#if 0
    for(auto mon : aimList) {
        mon->showProperties();
    }

    for(auto mon : amList) {
        mon->showProperties();
    }

    for(auto mon : asmList) {
        mon->showProperties();
    }
    if(fifoCtrl) fifoCtrl->showProperties();
    if(fifoRead) fifoRead->showProperties();
    if(traceDMA) traceDMA->showProperties();
    if(traceFunnel) traceFunnel->showProperties();
#endif

    mIsDebugIPlayoutRead = true;
  }

  void DeviceIntf::configureDataflow(bool* ipConfig)
  {
    // this ipConfig only tells whether the corresponding CU has ap_control_chain :
    // could have been just a property on the monitor set at compile time (in debug_ip_layout)
    if(!ipConfig)
      return;

    uint32_t i = 0;
    for(auto mon: amList) {
      mon->configureDataflow(ipConfig[i++]);
    }
  }

  void DeviceIntf::configAmContext(const std::string& ctx_info)
  {
    if (ctx_info.empty())
      return;
    for (auto mon : amList) {
      mon->disable();
    }
  }

  size_t DeviceIntf::allocTraceBuf(uint64_t sz ,uint8_t memIdx)
  {
    auto bufHandle = mDevice->alloc(sz, memIdx);
    // Can't read a buffer xrt hasn't written to
    mDevice->sync(bufHandle, sz, 0, xdp::Device::direction::HOST2DEVICE);
    return bufHandle;
  }

  void DeviceIntf::freeTraceBuf(size_t bufHandle)
  {
    mDevice->free(bufHandle);
  }

  /**
  * Takes the offset inside the mapped buffer
  * and syncs it with device and returns its virtual address.
  * We can read the entire buffer in one go if we want to
  * or choose to read in chunks
  */
  void* DeviceIntf::syncTraceBuf(size_t bufHandle ,uint64_t offset, uint64_t bytes)
  {
    auto addr = mDevice->map(bufHandle);
    if (!addr)
      return nullptr;
    mDevice->sync(bufHandle, bytes, offset, xdp::Device::direction::DEVICE2HOST);
    return static_cast<char*>(addr) + offset;
  }

  uint64_t DeviceIntf::getDeviceAddr(size_t bufHandle)
  {
    return mDevice->getDeviceAddr(bufHandle);
  }

  void DeviceIntf::initTS2MM(uint64_t bufSz, uint64_t bufAddr)
  {
    if (traceDMA)
      traceDMA->init(bufSz, bufAddr);
  }

  uint64_t DeviceIntf::getWordCountTs2mm()
  {
    if (traceDMA)
      return traceDMA->getWordCount();
    return 0;
  }

  uint8_t DeviceIntf::getTS2MmMemIndex()
  {
    if (traceDMA)
      return traceDMA->getMemIndex();
    return 0;
  }

  void DeviceIntf::resetTS2MM()
  {
    if (traceDMA)
      traceDMA->reset();
  }

  void DeviceIntf::parseTraceData(void* traceData, uint64_t bytes, xclTraceResultsVector& traceVector)
  {
    if (traceDMA)
      traceDMA->parseTraceBuf(traceData, bytes, traceVector);
  }

  void DeviceIntf::setMaxBwRead()
  {
    m_bw_read = mDevice->getMaxBwRead();
  }

  void DeviceIntf::setMaxBwWrite()
  {
    m_bw_read = mDevice->getMaxBwWrite();
  }

} // namespace xdp
