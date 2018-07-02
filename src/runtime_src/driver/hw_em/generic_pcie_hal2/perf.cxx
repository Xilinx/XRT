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

/*
 * Copyright (C) 2015 Xilinx, Inc
 * Author: Paul Schumacher
 * Performance Monitoring using PCIe for XDMA HAL Driver
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

#include "shim.h"
//#include "../user_common/perfmon_parameters.h"
#include "xclbin.h"

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>

#include <iostream>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <thread>
#include <vector>
#include <time.h>
#include <string>

#ifndef _WINDOWS
// TODO: Windows build support
//    unistd.h is linux only header file
//    it is included for read, write, close, lseek64
#include <unistd.h>
#endif

#ifdef _WINDOWS
#define __func__ __FUNCTION__
#endif

namespace xclhwemhal2 {

  // ****************
  // Helper functions
  // ****************

  double HwEmShim::xclGetDeviceClockFreqMHz()
  {
    //return 1.0;
    double clockSpeed;
    //300.0 MHz
    clockSpeed = 300.0;
    return clockSpeed;
  }

  // Get the maximum bandwidth for host reads from the device (in MB/sec)
  // NOTE: for now, just return 8.0 GBps (the max achievable for PCIe Gen3)
  double HwEmShim::xclGetReadMaxBandwidthMBps() {
    return 8000.0;
  }

  // Get the maximum bandwidth for host writes to the device (in MB/sec)
  // NOTE: for now, just return 8.0 GBps (the max achievable for PCIe Gen3)
  double HwEmShim::xclGetWriteMaxBandwidthMBps() {
    return 8000.0;
  }

  size_t HwEmShim::xclPerfMonClockTraining()
  {
    return 0;
  }

  size_t HwEmShim::xclPerfMonStartCounters()
  {
    //TODO::Still to decide whether to start Performance Monitor or not
    return 0;
  }

  size_t HwEmShim::xclPerfMonStopCounters()
  {
    //TODO::Still to decide whether to stop Performance Monitor or not
    return 0;
  }

  uint32_t HwEmShim::getPerfMonNumberSlots(xclPerfMonType type) {
    if (type == XCL_PERF_MON_MEMORY)
      return mMemoryProfilingNumberSlots;
    if (type == XCL_PERF_MON_ACCEL)
      return mAccelProfilingNumberSlots;
    if (type == XCL_PERF_MON_STALL)
      return mStallProfilingNumberSlots;
    if (type == XCL_PERF_MON_HOST)
      return 1;

    return 0;
  }

  // Get slot name
  void HwEmShim::getPerfMonSlotName(xclPerfMonType type, uint32_t slotnum,
   		                            char* slotName, uint32_t length) {
    std::string str = "";
    if (type == XCL_PERF_MON_MEMORY) {
      str = (slotnum < XSPM_MAX_NUMBER_SLOTS) ? mPerfMonSlotName[slotnum] : "";
    }
    if (type == XCL_PERF_MON_ACCEL) {
      str = (slotnum < XSAM_MAX_NUMBER_SLOTS) ? mAccelMonSlotName[slotnum] : "";
    }
    strncpy(slotName, str.c_str(), length);
  }

  size_t HwEmShim::xclGetDeviceTimestamp()
  {
    bool ack = true;
    size_t deviceTimeStamp = 0;
    xclGetDeviceTimestamp_RPC_CALL(xclGetDeviceTimestamp,ack,deviceTimeStamp);
    return deviceTimeStamp;
  }

  // ********
  // Counters
  // ********

  size_t HwEmShim::xclPerfMonReadCounters(xclPerfMonType type, xclCounterResults& counterResults)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

    // Initialize all values in struct to 0
    memset(&counterResults, 0, sizeof(xclCounterResults));

    // TODO: modify if adding support for multiple DDRs, where # slots = # DDR + 1
    //counterResults.NumSlots = 2;

    // TODO: support other profiling
    if (type != XCL_PERF_MON_MEMORY && type != XCL_PERF_MON_ACCEL) {
      PRINTENDFUNC;
      return 0;
    }
    bool accel = (type==XCL_PERF_MON_ACCEL) ? true : false;

    //TODO::Need to call for each slot individually
    //Right now we have only one slot
    //sampleIntervalUsec is not used yet. Need to identify how to use this
    uint32_t wr_byte_count = 0;
    uint32_t wr_trans_count = 0;
    uint32_t total_wr_latency = 0;
    uint32_t rd_byte_count = 0;
    uint32_t rd_trans_count = 0;
    uint32_t total_rd_latency= 0;

    if (simulator_started == true) {
#ifndef _WINDOWS
      // TODO: Windows build support
      // *_RPC_CALL uses unix_socket
      uint32_t counter = 0;
      uint32_t numSlots = getPerfMonNumberSlots(type);
      //counterResults.NumSlots = numSlots;
      for(; counter < numSlots; counter++)
      {
        if (counter == XPAR_SPM0_HOST_SLOT && !accel) // Ignore host slot
          continue;
        char slotname[128];
        getPerfMonSlotName(type,counter,slotname,128);
        xclPerfMonReadCounters_RPC_CALL(xclPerfMonReadCounters,wr_byte_count,wr_trans_count,total_wr_latency,rd_byte_count,rd_trans_count,total_rd_latency,sampleIntervalUsec,slotname,accel);
#endif
        if (!accel) {
          counterResults.WriteBytes[counter] = wr_byte_count;
          counterResults.WriteTranx[counter] = wr_trans_count;
          counterResults.WriteLatency[counter] = total_wr_latency;
          counterResults.ReadBytes[counter] = rd_byte_count;
          counterResults.ReadTranx[counter] = rd_trans_count;
          counterResults.ReadLatency[counter] = total_rd_latency;
        }
        else {
          counterResults.CuExecCount[counter] = rd_byte_count;
          counterResults.CuExecCycles[counter] = total_wr_latency;
          counterResults.CuMinExecCycles[counter] = rd_trans_count;
          counterResults.CuMaxExecCycles[counter] = total_rd_latency;
          //counterResults.CuStallIntCycles[counter] = total_int_stalls;
          //counterResults.CuStallStrCycles[counter] = total_str_stalls;
          //counterResults.CuStallExtCycles[counter] = total_ext_stalls;
        }
      }
    }

    PRINTENDFUNC;
    return(XPAR_AXI_PERF_MON_0_NUMBER_SLOTS * XAPM_METRIC_COUNTERS_PER_SLOT);
  }

  // *****
  // Trace
  // *****
  size_t HwEmShim::xclPerfMonStartTrace(uint32_t startTrigger)
  {
    return 0;
  }

  size_t HwEmShim::xclPerfMonStopTrace()
  {
    return 0;
  }

  uint32_t HwEmShim::xclPerfMonGetTraceCount(xclPerfMonType type)
  {
    // TODO: support other profiling
    if (type != XCL_PERF_MON_MEMORY && type != XCL_PERF_MON_ACCEL)
      return 0;
    bool accel = (type==XCL_PERF_MON_ACCEL) ? true : false;
    uint32_t no_of_final_samples = 0;

    if(tracecount_calls < xclemulation::config::getInstance()->getMaxTraceCount())
    {
      tracecount_calls = tracecount_calls + 1;
      return 0;
    }
    tracecount_calls = 0;
    uint32_t numSlots = getPerfMonNumberSlots(type);
    bool ack = true;
    for(unsigned int counter = 0; counter < numSlots; counter++)
    {
      if (counter == XPAR_SPM0_HOST_SLOT && !accel)
        continue;
      uint32_t no_of_samples = 0;

      if (simulator_started == true)
      {
#ifndef _WINDOWS
        // TODO: Windows build support
        // *_RPC_CALL uses unix_socket
        char slotname[128];
        getPerfMonSlotName(type,counter,slotname,128);

        xclPerfMonGetTraceCount_RPC_CALL(xclPerfMonGetTraceCount,ack,no_of_samples,slotname,accel);
#endif
      }
      no_of_final_samples = no_of_samples + list_of_events[counter].size();
    }
    return no_of_final_samples+1000;
  }

  size_t HwEmShim::xclPerfMonReadTrace(xclPerfMonType type, xclTraceResultsVector& traceVector)
  {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << type << std::endl;
    }

    // TODO: support other profiling
    if (type != XCL_PERF_MON_MEMORY && type != XCL_PERF_MON_ACCEL) {
      traceVector.mLength = 0;
      return 0;
    }
    bool accel = (type==XCL_PERF_MON_ACCEL) ? true : false;

    uint32_t counter = 0;
    uint32_t numSlots = getPerfMonNumberSlots(type);
    bool ack = true;
    unsigned int index = 0;
    for(; counter < numSlots; counter++)
    {
      // Ignore host
      if (counter == XPAR_SPM0_HOST_SLOT && !accel)
        continue;

      unsigned int numberOfElementsAdded = 0;

      if(list_of_events[counter].size() > 0 && index<(MAX_TRACE_NUMBER_SAMPLES-7))
      {
        std::vector<Event>::iterator startEvent = list_of_events[counter].begin();
        std::vector<Event>::iterator endEvent = list_of_events[counter].end();

        for(;startEvent != endEvent && index<(MAX_TRACE_NUMBER_SAMPLES-7) ;startEvent++)
        {
          numberOfElementsAdded = numberOfElementsAdded+1;
          Event currentEvent = *startEvent;

          xclTraceResults result;
          memset(&result, 0, sizeof(xclTraceResults));
          result.TraceID = accel ? counter + 64 : counter * 2;
          result.Timestamp = currentEvent.timestamp;
          result.Overflow = (currentEvent.timestamp >> 17) & 0x1;
          result.EventFlags = currentEvent.eventflags;
          result.ReadAddrLen = currentEvent.arlen;
          result.WriteAddrLen = currentEvent.awlen;
          result.WriteBytes = (currentEvent.writeBytes);
          result.ReadBytes  = (currentEvent.readBytes);
          result.HostTimestamp = currentEvent.host_timestamp;
          result.EventID = XCL_PERF_MON_HW_EVENT;
          traceVector.mArray[index++] = result;
        }
        list_of_events[counter].erase(list_of_events[counter].begin(),list_of_events[counter].begin() + numberOfElementsAdded);
        traceVector.mLength = index;
      }

      if (simulator_started == true)
      {
        unsigned int samplessize = 0;
#ifndef _WINDOWS
        // TODO: Windows build support
        // *_RPC_CALL uses unix_socket
        char slotname[128];
        getPerfMonSlotName(type,counter,slotname,128);
        xclPerfMonReadTrace_RPC_CALL(xclPerfMonReadTrace,ack,samplessize,slotname,accel);
#endif
        unsigned int i = 0;
        for(; i<samplessize && index<(MAX_TRACE_NUMBER_SAMPLES-7); i++)
        {
#ifndef _WINDOWS
          // TODO: Windows build support
          // r_msg is defined as part of *RPC_CALL definition
          const xclPerfMonReadTrace_response::events &event = r_msg.output_data(i);

          xclTraceResults result;
          memset(&result, 0, sizeof(xclTraceResults));
          result.TraceID = accel ? counter + 64 : counter * 2;
          result.Timestamp = event.timestamp();
          result.Overflow = (event.timestamp() >> 17) & 0x1;
          result.EventFlags = event.eventflags();
          result.ReadAddrLen = event.arlen();
          result.WriteAddrLen = event.awlen();
          result.WriteBytes = (event.wr_bytes());
          result.ReadBytes  = (event.rd_bytes());
          result.HostTimestamp = event.host_timestamp();
          result.EventID = XCL_PERF_MON_HW_EVENT;
          traceVector.mArray[index++] = result;
#endif
        }
        traceVector.mLength = index;

        Event eventObj;
        for(; i<samplessize ; i++)
        {
#ifndef _WINDOWS
          // TODO: Windows build support
          // r_msg is defined as part of *RPC_CALL definition
          const xclPerfMonReadTrace_response::events &event = r_msg.output_data(i);
          eventObj.timestamp = event.timestamp();
          eventObj.eventflags = event.eventflags();
          eventObj.arlen = event.arlen();
          eventObj.awlen = event.awlen();
          eventObj.host_timestamp = event.host_timestamp();
          eventObj.readBytes = event.rd_bytes();
          eventObj.writeBytes = event.wr_bytes();
          list_of_events[counter].push_back(eventObj);
#endif
        }
      }
    }

    if (mLogStream.is_open()) {
      mLogStream << "[xclPerfMonReadTrace] trace vector length = " << traceVector.mLength << std::endl;
    }
    PRINTENDFUNC;
    return traceVector.mLength;
  }

} // namespace xclhwemhal2
