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

#include "device_intf.h"
#include "xclperf.h"
#include "xcl_perfmon_parameters.h"
#include "xrt/device/device.h"

#include <iostream>
#include <cstdio>
#include <cstring>
//#include <algorithm>
#include <thread>
#include <vector>
//#include <time.h>
#include <string.h>
#include <chrono>

#ifndef _WINDOWS
// TODO: Windows build support
//    unistd.h is linux only header file
//    it is included for read, write, close, lseek64
#include <unistd.h>
#endif

#ifdef _WINDOWS
#define __func__ __FUNCTION__
#endif

namespace xdp {

DeviceIntf::~DeviceIntf()
{
    for(std::vector<AIM*>::iterator itr = aimList.begin(); itr != aimList.end(); ++itr) {
        delete (*itr);  // delete the object
        (*itr) = nullptr;
    }
    for(std::vector<AM*>::iterator itr = amList.begin(); itr != amList.end(); ++itr) {
        delete (*itr);  // delete the object
        (*itr) = nullptr;
    }
    for(std::vector<ASM*>::iterator itr = asmList.begin(); itr != asmList.end(); ++itr) {
        delete (*itr);  // delete the object
        (*itr) = nullptr;
    }
    aimList.clear();
    amList.clear();
    asmList.clear();

    delete fifoCtrl;
    delete fifoRead;
    delete traceFunnel;
    delete traceDMA;
}

  // ***************************************************************************
  // Read/Write
  // ***************************************************************************

#if 0
  size_t DeviceIntf::write(uint64_t offset, const void *hostBuf, size_t size)
  {
    if (mDeviceHandle == nullptr)
      return 0;
	return xclWrite(mDeviceHandle, XCL_ADDR_SPACE_DEVICE_PERFMON, offset, hostBuf, size);
  }

  size_t DeviceIntf::read(uint64_t offset, void *hostBuf, size_t size)
  {
    if (mDeviceHandle == nullptr)
      return 0;
	return xclRead(mDeviceHandle, XCL_ADDR_SPACE_DEVICE_PERFMON, offset, hostBuf, size);
  }

  size_t DeviceIntf::traceRead(void *buffer, size_t size, uint64_t addr)
  {
    if (mDeviceHandle == nullptr)
      return 0;
    return xclUnmgdPread(mDeviceHandle, 0, buffer, size, addr);
  }
#endif

  // ***************************************************************************
  // Generic Helper functions
  // ***************************************************************************

#if 0
  // Get host timestamp to write to monitors
  // IMPORTANT NOTE: this *must* be compatible with the method of generating
  // timestamps as defined in RTProfile::getTraceTime()
  uint64_t DeviceIntf::getHostTraceTimeNsec()
  {
    return 0;
#if 0
    using namespace std::chrono;
    typedef duration<uint64_t, std::ratio<1, 1000000000>> duration_ns;
    duration_ns time_span =
        duration_cast<duration_ns>(high_resolution_clock::now().time_since_epoch());
    return time_span.count();
#endif
  }

  // Convert decimal to binary string
  // NOTE: length of string is always sizeof(uint32_t) * 8
  std::string DeviceIntf::dec2bin(uint32_t n) {
    char result[(sizeof(uint32_t) * 8) + 1];
    unsigned index = sizeof(uint32_t) * 8;
    result[index] = '\0';

    do {
      result[ --index ] = '0' + (n & 1);
    } while (n >>= 1);

    for (int i=index-1; i >= 0; --i)
      result[i] = '0';

    return std::string( result );
  }

  // Convert decimal to binary string of length bits
  std::string DeviceIntf::dec2bin(uint32_t n, unsigned bits) {
    char result[bits + 1];
    unsigned index = bits;
    result[index] = '\0';

    do result[ --index ] = '0' + (n & 1);
    while (n >>= 1);

    for (int i=index-1; i >= 0; --i)
      result[i] = '0';

    return std::string( result );
  }

  uint32_t DeviceIntf::getMaxSamples(xclPerfMonType type)
  {
    if (type == XCL_PERF_MON_MEMORY) return XPAR_AXI_PERF_MON_0_TRACE_NUMBER_SAMPLES;
    if (type == XCL_PERF_MON_HOST) return XPAR_AXI_PERF_MON_1_TRACE_NUMBER_SAMPLES;
    // TODO: get number of samples from metadata
    if (type == XCL_PERF_MON_ACCEL) return XPAR_AXI_PERF_MON_2_TRACE_NUMBER_SAMPLES;
    return 0;
  }
#endif


  void DeviceIntf::setDeviceHandle(void* xrtDevice)
  {
    if(!mDeviceHandle) {
      mDeviceHandle = xrtDevice;
      return;
    }
    if(mDeviceHandle != xrtDevice) {
      // ERROR : trying to set the device handle when it is already populated with some other device
    }
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
      for(std::vector<AM*>::iterator itr = amList.begin(); itr != amList.end(); ++itr) {
        if((*itr)->hasStall())  count++;
      }
      return count;
    }

    if(type == XCL_PERF_MON_HOST) {
      uint32_t count = 0;
      for(std::vector<AIM*>::iterator itr = aimList.begin(); itr != aimList.end(); ++itr) {
        if((*itr)->isHostMonitor())  count++;
      }
      return count;
    }

    // FIFO ?

    if(type == XCL_PERF_MON_SHELL) {
      uint32_t count = 0;
      for(std::vector<AIM*>::iterator itr = aimList.begin(); itr != aimList.end(); ++itr) {
        if((*itr)->isShellMonitor())  count++;
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

  uint32_t DeviceIntf::getMonitorProperties(xclPerfMonType type, uint32_t index)
  {
    if((type == XCL_PERF_MON_MEMORY) && (index < aimList.size())) { return aimList[index]->getProperties(); }
    if((type == XCL_PERF_MON_ACCEL)  && (index < amList.size()))  { return amList[index]->getProperties(); }
    if((type == XCL_PERF_MON_STR)    && (index < asmList.size())) { return asmList[index]->getProperties(); }
    return 0;
  }

  // ***************************************************************************
  // Counters
  // ***************************************************************************

  // Start device counters performance monitoring
  size_t DeviceIntf::startCounters(xclPerfMonType type)
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
                << type << ", Start device counters..." << std::endl;
    }

    std::cout << " In DeviceIntf::startCounters " << std::endl;
    // Update addresses for debug/profile IP
//    readDebugIPlayout();

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;

    // AIM
    for(std::vector<AIM*>::iterator itr = aimList.begin(); itr != aimList.end(); ++itr) {
        size += (*itr)->startCounter();
    }
    // AM
    for(std::vector<AM*>::iterator itr = amList.begin(); itr != amList.end(); ++itr) {
        size += (*itr)->startCounter();
    }

    // ASM
    for(std::vector<ASM*>::iterator itr = asmList.begin(); itr != asmList.end(); ++itr) {
        size += (*itr)->startCounter();
    }
    return size;
  }

  // Stop both profile and trace performance monitoring
  size_t DeviceIntf::stopCounters(xclPerfMonType type) {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
          << type << ", Stop and reset device counters..." << std::endl;
    }

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;

    // AIM
    for(std::vector<AIM*>::iterator itr = aimList.begin(); itr != aimList.end(); ++itr) {
        size += (*itr)->stopCounter();
    }


#if 0
    // why not these in the original code
    // AM
    for(std::vector<AM*>::iterator itr = amList.begin(); itr != amList.end(); ++itr) {
        size += (*itr)->stopCounter();
    }

    // ASM
    for(std::vector<ASM*>::iterator itr = asmList.begin(); itr != asmList.end(); ++itr) {
        size += (*itr)->stopCounter();
    }
#endif
    return size;
  }

  // Read AIM performance counters
  size_t DeviceIntf::readCounters(xclPerfMonType type, xclCounterResults& counterResults) {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
      << ", " << type << ", " << &counterResults
      << ", Read device counters..." << std::endl;
    }

    // Initialize all values in struct to 0
    memset(&counterResults, 0, sizeof(xclCounterResults));

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;

    // AIM
    uint32_t idx = 0;
    for(std::vector<AIM*>::iterator itr = aimList.begin(); itr != aimList.end(); ++itr, ++idx) {
        size += (*itr)->readCounter(counterResults, idx);
    }

    // AM
    idx = 0;
    for(std::vector<AM*>::iterator itr = amList.begin(); itr != amList.end(); ++itr, ++idx) {
        size += (*itr)->readCounter(counterResults, idx);
    }

    // ASM
    idx = 0;
    for(std::vector<ASM*>::iterator itr = asmList.begin(); itr != asmList.end(); ++itr, ++idx) {
        size += (*itr)->readCounter(counterResults, idx);
    }

    return size;
  }

  // ***************************************************************************
  // Timeline Trace
  // ***************************************************************************

  // Start trace performance monitoring
  size_t DeviceIntf::startTrace(xclPerfMonType type, uint32_t startTrigger)
  {
    // StartTrigger Bits:
    // Bit 0: Trace Coarse/Fine     Bit 1: Transfer Trace Ctrl
    // Bit 2: CU Trace Ctrl         Bit 3: INT Trace Ctrl
    // Bit 4: Str Trace Ctrl        Bit 5: Ext Trace Ctrl
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
                << ", " << type << ", " << startTrigger
                << ", Start device tracing..." << std::endl;
    }
    size_t size = 0;

    // check which of these IPs are trace enabled ?
    // AIM
    for(std::vector<AIM*>::iterator itr = aimList.begin(); itr != aimList.end(); ++itr) {
        size += (*itr)->triggerTrace(startTrigger);
    }
    // AM
    for(std::vector<AM*>::iterator itr = amList.begin(); itr != amList.end(); ++itr) {
        size += (*itr)->triggerTrace(startTrigger);
    }
    // ASM 
    for(std::vector<ASM*>::iterator itr = asmList.begin(); itr != asmList.end(); ++itr) {
        size += (*itr)->triggerTrace(startTrigger);
    }

// why is this done

    // Get number of trace samples and reset fifo
    getTraceCount(type /* does not matter */);  // get number of samples from Fifo Control
    // reset fifo
    fifoCtrl->reset();
    // Get number of trace samples 
    getTraceCount(type /* does not matter */);

    // TraceFunnel
    traceFunnel->initiateClockTraining();
    return size;
  }

  // Stop trace performance monitoring
  size_t DeviceIntf::stopTrace(xclPerfMonType type)
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id() << ", "
                << type << ", Stop and reset device tracing..." << std::endl;
    }

    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;

    getTraceCount(type /* does not matter */);
    size += fifoCtrl->reset();
    // fifoRead reset ?

    return size;
  }

  // Get trace word count
  uint32_t DeviceIntf::getTraceCount(xclPerfMonType type) {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
                << ", " << type << std::endl;
    }

    if (!mIsDeviceProfiling || !fifoCtrl)
   	  return 0;

    return fifoCtrl->getNumTraceSamples();
  }

  // Read all values from APM trace AXI stream FIFOs
  size_t DeviceIntf::readTrace(xclPerfMonType type, xclTraceResultsVector& traceVector)
  {
    if (mVerbose) {
      std::cout << __func__ << ", " << std::this_thread::get_id()
                << ", " << type << ", " << &traceVector
                << ", Reading device trace stream..." << std::endl;
    }

    traceVector.mLength = 0;
    if (!mIsDeviceProfiling)
   	  return 0;

    size_t size = 0;
    size += fifoRead->readTrace(traceVector, getTraceCount(type /*does not matter*/));

    return size;
  }

  void DeviceIntf::readDebugIPlayout()
  {
    if(mIsDebugIPlayoutRead || !mDeviceHandle)
        return;

    xrt::device* xrtDevice = (xrt::device*)mDeviceHandle;
    std::string path = xrtDevice->getDebugIPlayoutPath().get();
    if(path.empty()) {
        // error ? : for HW_emu this will be empty for now ; but as of current status should not have been called 
        return;
    }

    uint32_t liveProcessesOnDevice = xrtDevice->getNumLiveProcesses().get();
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
      for( unsigned int i = 0; i < map->m_count; i++ ) {
      switch(map->m_debug_ip_data[i].m_type) {
        case AXI_MM_MONITOR : aimList.push_back(new AIM(mDeviceHandle, i, &(map->m_debug_ip_data[i])));
                              break;
        case ACCEL_MONITOR  : amList.push_back(new AM(mDeviceHandle, i, &(map->m_debug_ip_data[i])));
                              break;
        case AXI_STREAM_MONITOR : asmList.push_back(new ASM(mDeviceHandle, i, &(map->m_debug_ip_data[i])));
                                  break;
        case AXI_MONITOR_FIFO_LITE : fifoCtrl = new TraceFifoLite(mDeviceHandle, i, &(map->m_debug_ip_data[i]));
                                     break;
        case AXI_MONITOR_FIFO_FULL : fifoRead = new TraceFifoFull(mDeviceHandle, i, &(map->m_debug_ip_data[i]));
                                     break;
        case AXI_TRACE_FUNNEL : traceFunnel = new TraceFunnel(mDeviceHandle, i, &(map->m_debug_ip_data[i]));
                                break;
//        case TRACE_S2MM : traceDMA = new TraceS2MM(mDeviceHandle, i, &(map->m_debug_ip_data[i]));
//                          break;
        default : break;
        // case AXI_STREAM_PROTOCOL_CHECKER

      }
     }
    }
    ifs.close();
#if 0
    for(std::vector<AIM*>::iterator itr = aimList.begin(); itr != aimList.end(); ++itr) {
        (*itr)->showProperties();
    }

    for(std::vector<AM*>::iterator itr = amList.begin(); itr != amList.end(); ++itr) {
        (*itr)->showProperties();
    }

    for(std::vector<ASM*>::iterator itr = asmList.begin(); itr != asmList.end(); ++itr) {
        (*itr)->showProperties();
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
    for(std::vector<AM*>::iterator itr = amList.begin(); itr != amList.end(); ++itr, ++i) {
        (*itr)->configureDataflow(ipConfig[i]);
    }
  }

} // namespace xdp
