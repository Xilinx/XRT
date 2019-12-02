/*
 * Copyright (C) 2017-2018 Xilinx, Inc
 * Performance Monitoring using PCIe for AWS HAL Driver
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
#include "xcl_perfmon_parameters.h"
#include "xclperf.h"
#include "core/pcie/driver/linux/include/xocl_ioctl.h"
#include "core/pcie/driver/linux/include/mgmt-reg.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"
#include "core/common/AlignedAllocator.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <thread>
#include <vector>
#include <chrono>
#include <ctime>
#include <cstring>

#ifdef _WIN32
#define __func__ __FUNCTION__
#endif

namespace awsbwhal {

  static int unmgdPread(int fd, void *buffer, size_t size, uint64_t addr)
  {
    drm_xocl_pread_unmgd unmgd = { 0, 0, addr, size, reinterpret_cast<uint64_t>(buffer) };
    return ioctl(fd, DRM_IOCTL_XOCL_PREAD_UNMGD, &unmgd);
  }

  // ****************
  // Helper functions
  // ****************

  bool AwsXcl::isDSAVersion(unsigned majorVersion, unsigned minorVersion, bool onlyThisVersion) {
    unsigned checkVersion = (majorVersion << 4) + (minorVersion);
    if (onlyThisVersion)
      return (mDeviceInfo.mDeviceVersion == checkVersion);
    return (mDeviceInfo.mDeviceVersion >= checkVersion);
  }

  unsigned AwsXcl::getBankCount() {
    return mDeviceInfo.mDDRBankCount;
  }

  // Set number of profiling slots in monitor
  // NOTE: not supported anymore (extracted from debug_ip_layout)
  void AwsXcl::xclSetProfilingNumberSlots(xclPerfMonType type, uint32_t numSlots) {
    // do nothing (extracted from debug_ip_layout)
  }

  // Get host timestamp to write to APM
  // IMPORTANT NOTE: this *must* be compatible with the method of generating
  // timestamps as defined in RTProfile::getTraceTime()
  uint64_t AwsXcl::getHostTraceTimeNsec() {
   using namespace std::chrono;
   typedef duration<uint64_t, std::ratio<1, 1000000000>> duration_ns;
   duration_ns time_span =
       duration_cast<duration_ns>(high_resolution_clock::now().time_since_epoch());
   return time_span.count();
  }

  uint64_t AwsXcl::getPerfMonBaseAddress(xclPerfMonType type, uint32_t slotNum) {
    if (type == XCL_PERF_MON_MEMORY) return mPerfMonBaseAddress[slotNum];
    if (type == XCL_PERF_MON_ACCEL)  return mAccelMonBaseAddress[slotNum];
    if (type == XCL_PERF_MON_STR)    return mStreamMonBaseAddress[slotNum];
    return 0;
  }

  uint64_t AwsXcl::getPerfMonFifoBaseAddress(xclPerfMonType type, uint32_t fifonum) {
   if (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_ACCEL)
       return mPerfMonFifoCtrlBaseAddress;
   else
     return 0;
  }

  uint64_t AwsXcl::getPerfMonFifoReadBaseAddress(xclPerfMonType type, uint32_t fifonum) {
   if (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_ACCEL)
       return mPerfMonFifoReadBaseAddress;
   else
     return 0;
  }

  uint64_t AwsXcl::getTraceFunnelAddress(xclPerfMonType type) {
    if (type == XCL_PERF_MON_MEMORY || type == XCL_PERF_MON_ACCEL)
        return mTraceFunnelAddress;
    else
      return 0;
  }

  uint32_t AwsXcl::getPerfMonProperties(xclPerfMonType type, uint32_t slotnum) {
    if (type == XCL_PERF_MON_MEMORY && slotnum < XAIM_MAX_NUMBER_SLOTS)
      return static_cast<uint32_t>(mPerfmonProperties[slotnum]);
    if (type == XCL_PERF_MON_ACCEL && slotnum < XAM_MAX_NUMBER_SLOTS)
        return static_cast<uint32_t>(mAccelmonProperties[slotnum]);
    if (type == XCL_PERF_MON_STR && slotnum < XASM_MAX_NUMBER_SLOTS)
      return static_cast<uint32_t>(mStreammonProperties[slotnum]);
// Trace Fifo not added yet
//    if (type == XCL_PERF_MON_FIFO)
//      return static_cast<uint32_t>(mTraceFifoProperties);
    return 0;
  }

  uint32_t AwsXcl::getPerfMonNumberSlots(xclPerfMonType type) {
    if (type == XCL_PERF_MON_MEMORY)
      return mMemoryProfilingNumberSlots;
    if (type == XCL_PERF_MON_ACCEL)
      return mAccelProfilingNumberSlots;
    if (type == XCL_PERF_MON_STALL)
      return mStallProfilingNumberSlots;
    if (type == XCL_PERF_MON_HOST) {
      uint32_t count = 0;
      for (unsigned int i=0; i < mMemoryProfilingNumberSlots; i++) {
        if (mPerfmonProperties[i] & XAIM_HOST_PROPERTY_MASK) count++;
      }
      return count;
    }
    return 0;
  }

  void AwsXcl::getPerfMonSlotName(xclPerfMonType type, uint32_t slotnum,
                                   char* slotName, uint32_t length) {
    std::string str = "";
    if (type == XCL_PERF_MON_MEMORY) {
      str = (slotnum < XAIM_MAX_NUMBER_SLOTS) ? mPerfMonSlotName[slotnum] : "";
    }
    if (type == XCL_PERF_MON_ACCEL) {
      str = (slotnum < XAM_MAX_NUMBER_SLOTS) ? mAccelMonSlotName[slotnum] : "";
    }
    strncpy(slotName, str.c_str(), length);
  }

  uint32_t AwsXcl::getPerfMonNumberSamples(xclPerfMonType type) {
    if (type == XCL_PERF_MON_MEMORY) return XPAR_AXI_PERF_MON_0_TRACE_NUMBER_SAMPLES;
    if (type == XCL_PERF_MON_HOST) return XPAR_AXI_PERF_MON_1_TRACE_NUMBER_SAMPLES;
    // TODO: get number of samples from metadata
    if (type == XCL_PERF_MON_ACCEL) return XPAR_AXI_PERF_MON_2_TRACE_NUMBER_SAMPLES;
    return 0;
  }

  uint8_t AwsXcl::getPerfMonShowIDS(xclPerfMonType type) {
    if (type == XCL_PERF_MON_MEMORY) {
      if (isDSAVersion(1, 0, true))
        return 0;
      if (getBankCount() > 1)
        return XPAR_AXI_PERF_MON_0_SHOW_AXI_IDS_2DDR;
      return XPAR_AXI_PERF_MON_0_SHOW_AXI_IDS;
    }
    if (type == XCL_PERF_MON_HOST) {
      return XPAR_AXI_PERF_MON_1_SHOW_AXI_IDS;
    }
    // TODO: get show IDs
    if (type == XCL_PERF_MON_ACCEL) {
      return XPAR_AXI_PERF_MON_2_SHOW_AXI_IDS;
    }
    return 0;
  }

  uint8_t AwsXcl::getPerfMonShowLEN(xclPerfMonType type) {
    if (type == XCL_PERF_MON_MEMORY) {
      if (getBankCount() > 1)
        return XPAR_AXI_PERF_MON_0_SHOW_AXI_LEN_2DDR;
      return XPAR_AXI_PERF_MON_0_SHOW_AXI_LEN;
    }
    if (type == XCL_PERF_MON_HOST) {
      return XPAR_AXI_PERF_MON_1_SHOW_AXI_LEN;
    }
    // TODO: get show IDs
    if (type == XCL_PERF_MON_ACCEL) {
      return XPAR_AXI_PERF_MON_2_SHOW_AXI_LEN;
    }
    return 0;
  }

  uint32_t AwsXcl::getPerfMonSlotStartBit(xclPerfMonType type, uint32_t slotnum) {
    // NOTE: ID widths also set to 5 in HEAD/data/sdaccel/board_support/alpha_data/common/xclplat/xclplat_ip.tcl
    uint32_t bitsPerID = 5;
    uint8_t showIDs = getPerfMonShowIDS(type);
    uint8_t showLen = getPerfMonShowLEN(type);
    uint32_t bitsPerSlot = 10 + (bitsPerID * 4 * showIDs) + (16 * showLen);
    return (18 + (bitsPerSlot * slotnum));
  }

  uint32_t AwsXcl::getPerfMonSlotDataWidth(xclPerfMonType type, uint32_t slotnum) {
    // TODO: this only supports slot 0
    if (slotnum == 0) return XPAR_AXI_PERF_MON_0_SLOT0_DATA_WIDTH;
    if (slotnum == 1) return XPAR_AXI_PERF_MON_0_SLOT1_DATA_WIDTH;
    if (slotnum == 2) return XPAR_AXI_PERF_MON_0_SLOT2_DATA_WIDTH;
    if (slotnum == 3) return XPAR_AXI_PERF_MON_0_SLOT3_DATA_WIDTH;
    if (slotnum == 4) return XPAR_AXI_PERF_MON_0_SLOT4_DATA_WIDTH;
    if (slotnum == 5) return XPAR_AXI_PERF_MON_0_SLOT5_DATA_WIDTH;
    if (slotnum == 6) return XPAR_AXI_PERF_MON_0_SLOT6_DATA_WIDTH;
    if (slotnum == 7) return XPAR_AXI_PERF_MON_0_SLOT7_DATA_WIDTH;
    return XPAR_AXI_PERF_MON_0_SLOT0_DATA_WIDTH;
  }

  // Get the device clock frequency (in MHz)
  double AwsXcl::xclGetDeviceClockFreqMHz() {
    xclGetDeviceInfo2(&mDeviceInfo);
    unsigned clockFreq = mDeviceInfo.mOCLFrequency[0];
    if (clockFreq == 0)
      clockFreq = 300;

    //if (mLogStream.is_open())
    //  mLogStream << __func__ << ": clock freq = " << clockFreq << std::endl;
    return ((double)clockFreq);
  }

  // Get the maximum bandwidth for host reads from the device (in MB/sec)
  // NOTE: for now, set to: (256/8 bytes) * 300 MHz = 9600 MBps
  double AwsXcl::xclGetReadMaxBandwidthMBps() {
    return 9600.0;
  }

  // Get the maximum bandwidth for host writes to the device (in MB/sec)
  // NOTE: for now, set to: (256/8 bytes) * 300 MHz = 9600 MBps
  double AwsXcl::xclGetWriteMaxBandwidthMBps() {
    return 9600.0;
  }

  // Convert binary string to decimal
  uint32_t AwsXcl::bin2dec(std::string str, int start, int number) {
    return bin2dec(str.c_str(), start, number);
  }

  // Convert binary char * to decimal
  uint32_t AwsXcl::bin2dec(const char* ptr, int start, int number) {
    const char* temp_ptr = ptr + start;
    uint32_t value = 0;
    int i = 0;

    do {
      if (*temp_ptr != '0' && *temp_ptr!= '1')
        return value;
      value <<= 1;
      if(*temp_ptr=='1')
        value += 1;
      i++;
      temp_ptr++;
    } while (i < number);

    return value;
  }

  // Convert decimal to binary string
  // NOTE: length of string is always sizeof(uint32_t) * 8
  std::string AwsXcl::dec2bin(uint32_t n) {
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
  std::string AwsXcl::dec2bin(uint32_t n, unsigned bits) {
    char result[bits + 1];
    unsigned index = bits;
    result[index] = '\0';

    do result[ --index ] = '0' + (n & 1);
    while (n >>= 1);

    for (int i=index-1; i >= 0; --i)
      result[i] = '0';

    return std::string( result );
  }

  // Reset all APM trace AXI stream FIFOs
  size_t AwsXcl::resetFifos(xclPerfMonType type) {
    uint64_t resetCoreAddress = getPerfMonFifoBaseAddress(type, 0) + AXI_FIFO_SRR;
    uint64_t resetFifoAddress = getPerfMonFifoBaseAddress(type, 0) + AXI_FIFO_RDFR;

    size_t size = 0;
    uint32_t regValue = AXI_FIFO_RESET_VALUE;

    size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, resetCoreAddress, &regValue, 4);
    size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, resetFifoAddress, &regValue, 4);
    return size;
  }

  void AwsXcl::xclPerfMonConfigureDataflow(xclPerfMonType type, unsigned *ip_config) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", "
          << type << ", Configure Monitors For Dataflow..." << std::endl;
    }
    readDebugIpLayout();
    if (!mIsDeviceProfiling)
      return;

    uint32_t numSlots = getPerfMonNumberSlots(type);

    if (type == XCL_PERF_MON_ACCEL) {
      for (uint32_t i=0; i < numSlots; i++) {
        if (!ip_config[i]) continue;
        uint64_t baseAddress = getPerfMonBaseAddress(type,i);
        uint32_t regValue = 0;
        xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAM_CONTROL_OFFSET, &regValue, 4);
        regValue = regValue | XAM_DATAFLOW_EN_MASK;
        xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAM_CONTROL_OFFSET, &regValue, 4);
        if (mLogStream.is_open()) {
          mLogStream << "Dataflow enabled on slot : " << i << std::endl;
        }
      }
    }
  }

  // ********
  // Counters
  // ********

  // Start device counters performance monitoring
  size_t AwsXcl::xclPerfMonStartCounters(xclPerfMonType type) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", "
        << type << ", Start device counters..." << std::endl;
    }

    // Update addresses for debug/profile IP
    readDebugIpLayout();

    if (!mIsDeviceProfiling)
      return 0;

    size_t size = 0;
    uint32_t regValue;
    uint64_t baseAddress;
    uint32_t numSlots = getPerfMonNumberSlots(type);

    for (uint32_t i = 0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(type, i);

      // 1. Reset AXI - MM monitor metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAIM_CONTROL_OFFSET, &regValue, 4);

      regValue = regValue | XAIM_CR_COUNTER_RESET_MASK;
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAIM_CONTROL_OFFSET, &regValue, 4);

      regValue = regValue & ~(XAIM_CR_COUNTER_RESET_MASK);
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAIM_CONTROL_OFFSET, &regValue, 4);

      // 2. Start AXI-MM monitor metric counters
      regValue = regValue | XAIM_CR_COUNTER_ENABLE_MASK;
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAIM_CONTROL_OFFSET, &regValue, 4);

      // 3. Read from sample register to ensure total time is read again at end
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAIM_SAMPLE_OFFSET, &regValue, 4);
    }

    // Reset Accelerator Monitors
    type = XCL_PERF_MON_ACCEL;
    numSlots = getPerfMonNumberSlots(type);
    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(type,i);
      uint32_t origRegValue = 0;
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAM_CONTROL_OFFSET, &origRegValue, 4);
      regValue = origRegValue | XAM_COUNTER_RESET_MASK;
      // Reset begin
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAM_CONTROL_OFFSET, &regValue, 4);
      // Reset end
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAM_CONTROL_OFFSET, &origRegValue, 4);
    }

    // Reset AXI Stream Monitors
    type = XCL_PERF_MON_STR;
    numSlots = getPerfMonNumberSlots(type);
    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(type,i);
      uint32_t origRegValue = 0;
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XASM_CONTROL_OFFSET, &origRegValue, 4);
      regValue = origRegValue | XASM_COUNTER_RESET_MASK;
      // Reset begin
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XASM_CONTROL_OFFSET, &regValue, 4);
      // Reset end
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XASM_CONTROL_OFFSET, &origRegValue, 4);
    }

    return size;
  }

  // Stop both profile and trace performance monitoring
  size_t AwsXcl::xclPerfMonStopCounters(xclPerfMonType type) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", "
        << type << ", Stop and reset device counters..." << std::endl;
    }

    if (!mIsDeviceProfiling)
      return 0;

    size_t size = 0;
    uint32_t regValue;
    uint64_t baseAddress;
    uint32_t numSlots = getPerfMonNumberSlots(type);

    for (uint32_t i = 0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(type, i);

      // 1. Stop SPM metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAIM_CONTROL_OFFSET, &regValue, 4);

      regValue = regValue & ~(XAIM_CR_COUNTER_ENABLE_MASK);
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAIM_CONTROL_OFFSET, &regValue, 4);
    }
    return size;
  }

  signed AwsXcl::cmpMonVersions(unsigned major1, unsigned minor1, unsigned major2, unsigned minor2)
  {
    if (major2 > major1)
      return 1;
    else if (major2 < major1)
      return -1;
    else if (minor2 > minor1)
      return 1;
    else if (minor2 < minor1)
      return -1;
    else return 0;
  }

  // Read SPM performance counters
  size_t AwsXcl::xclPerfMonReadCounters(xclPerfMonType type, xclCounterResults& counterResults) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
        << ", " << type << ", " << &counterResults
        << ", Read device counters..." << std::endl;
    }

    // Initialize all values in struct to 0
    memset(&counterResults, 0, sizeof(xclCounterResults));

    if (!mIsDeviceProfiling)
      return 0;

    size_t size = 0;
    uint64_t baseAddress;
    uint32_t sampleInterval;
    uint32_t numSlots = 0;

    numSlots = getPerfMonNumberSlots(XCL_PERF_MON_MEMORY);
    for (uint32_t s=0; s < numSlots; s++) {
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_MEMORY,s);
      // Read sample interval register
      // NOTE: this also latches the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                    baseAddress + XAIM_SAMPLE_OFFSET,
                    &sampleInterval, 4);
      // Need to do this for every xilmon
      if (s==0){
        counterResults.SampleIntervalUsec = sampleInterval / xclGetDeviceClockFreqMHz();
      }

      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAIM_SAMPLE_WRITE_BYTES_OFFSET,
                      &counterResults.WriteBytes[s], 4);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAIM_SAMPLE_WRITE_TRANX_OFFSET,
                      &counterResults.WriteTranx[s], 4);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAIM_SAMPLE_WRITE_LATENCY_OFFSET,
                      &counterResults.WriteLatency[s], 4);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAIM_SAMPLE_READ_BYTES_OFFSET,
                      &counterResults.ReadBytes[s], 4);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAIM_SAMPLE_READ_TRANX_OFFSET,
                      &counterResults.ReadTranx[s], 4);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAIM_SAMPLE_READ_LATENCY_OFFSET,
                      &counterResults.ReadLatency[s], 4);

      // Read upper 32 bits (if available)
      if (mPerfmonProperties[s] & XAIM_64BIT_PROPERTY_MASK) {
        uint64_t upper[6] = {};
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAIM_SAMPLE_WRITE_BYTES_UPPER_OFFSET,
                        &upper[0], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAIM_SAMPLE_WRITE_TRANX_UPPER_OFFSET,
                        &upper[1], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAIM_SAMPLE_WRITE_LATENCY_UPPER_OFFSET,
                        &upper[2], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAIM_SAMPLE_READ_BYTES_UPPER_OFFSET,
                        &upper[3], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAIM_SAMPLE_READ_TRANX_UPPER_OFFSET,
                        &upper[4], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAIM_SAMPLE_READ_LATENCY_UPPER_OFFSET,
                        &upper[5], 4);

        counterResults.WriteBytes[s]   += (upper[0] << 32);
        counterResults.WriteTranx[s]   += (upper[1] << 32);
        counterResults.WriteLatency[s] += (upper[2] << 32);
        counterResults.ReadBytes[s]    += (upper[3] << 32);
        counterResults.ReadTranx[s]    += (upper[4] << 32);
        counterResults.ReadLatency[s]  += (upper[5] << 32);

        if (mLogStream.is_open()) {
          mLogStream << "AXI Interface Monitor Upper 32, slot " << s << std::endl;
          mLogStream << "  WriteBytes : " << upper[0] << std::endl;
          mLogStream << "  WriteTranx : " << upper[1] << std::endl;
          mLogStream << "  WriteLatency : " << upper[2] << std::endl;
          mLogStream << "  ReadBytes : " << upper[3] << std::endl;
          mLogStream << "  ReadTranx : " << upper[4] << std::endl;
          mLogStream << "  ReadLatency : " << upper[5] << std::endl;
        }
      }

      if (mLogStream.is_open()) {
        mLogStream << "Reading AXI Interface Monitor... SlotNum : " << s << std::endl;
        mLogStream << "Reading AXI Interface Monitor... WriteBytes : " << counterResults.WriteBytes[s] << std::endl;
        mLogStream << "Reading AXI Interface Monitor... WriteTranx : " << counterResults.WriteTranx[s] << std::endl;
        mLogStream << "Reading AXI Interface Monitor... WriteLatency : " << counterResults.WriteLatency[s] << std::endl;
        mLogStream << "Reading AXI Interface Monitor... ReadBytes : " << counterResults.ReadBytes[s] << std::endl;
        mLogStream << "Reading AXI Interface Monitor... ReadTranx : " << counterResults.ReadTranx[s] << std::endl;
        mLogStream << "Reading AXI Interface Monitor... ReadLatency : " << counterResults.ReadLatency[s] << std::endl;
      }
    }
    /*
     * Read Accelerator Monitor Data
     */
    numSlots = getPerfMonNumberSlots(XCL_PERF_MON_ACCEL);
    for (uint32_t s=0; s < numSlots; s++) {

      // Get Accelerator Monitor configuration
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_ACCEL,s);
      bool has64bit = (mAccelmonProperties[s] & XAM_64BIT_PROPERTY_MASK) ? true : false;
      // Accelerator Monitor > 1.1 supports dataflow monitoring
      bool hasDataflow = (cmpMonVersions(mAccelmonMajorVersions[s],mAccelmonMinorVersions[s],1,1) < 0) ? true : false;
      bool hasStall = (mAccelmonProperties[s] & XAM_STALL_PROPERTY_MASK) ? true : false;

      // Debug Info from first Accelerator Monitor
      if (mLogStream.is_open() && (s == 0)) {
        uint32_t version = 0;
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress, &version, 4);
        std::ios_base::fmtflags f(mLogStream.flags());
        mLogStream << "Accelerator Monitor slot " << s << " Base Address = 0x" << std::hex << baseAddress << std::endl;
        mLogStream.flags(f);
        mLogStream << "Accelerator Monitor Core Version Register : " << version << std::endl;
        mLogStream << "Accelerator Monitor Core Version Register : " << version << std::endl;
        mLogStream << "Accelerator Monitor Core vlnv : "
                   << " Major " << static_cast<int>(mAccelmonMajorVersions[s])
                   << " Minor " << static_cast<int>(mAccelmonMinorVersions[s])
                   << std::endl;
        mLogStream << "Accelerator Monitor config : "
                   << " 64 bit support : " << has64bit
                   << " Dataflow support : " << hasDataflow
                   << " Stall support : " << hasStall
                   << std::endl;
      }

      // Read sample interval register
      // NOTE: this also latches the sampled metric counters
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAM_SAMPLE_OFFSET,
                      &sampleInterval, 4);
      if (mLogStream.is_open()) {
        mLogStream << "Accelerator Monitor Sample Interval : " << sampleInterval << std::endl;
      }
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAM_ACCEL_EXECUTION_COUNT_OFFSET,
                      &counterResults.CuExecCount[s], 4);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAM_ACCEL_EXECUTION_CYCLES_OFFSET,
                      &counterResults.CuExecCycles[s], 4);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAM_ACCEL_MIN_EXECUTION_CYCLES_OFFSET,
                      &counterResults.CuMinExecCycles[s], 4);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAM_ACCEL_MAX_EXECUTION_CYCLES_OFFSET,
                      &counterResults.CuMaxExecCycles[s], 4);

      // Read upper 32 bits (if available)
      uint64_t upper[6] = {};
      if (has64bit) {
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAM_ACCEL_EXECUTION_COUNT_UPPER_OFFSET,
                        &upper[0], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAM_ACCEL_EXECUTION_CYCLES_UPPER_OFFSET,
                        &upper[1], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAM_ACCEL_MIN_EXECUTION_CYCLES_UPPER_OFFSET,
                        &upper[2], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAM_ACCEL_MAX_EXECUTION_CYCLES_UPPER_OFFSET,
                        &upper[3], 4);

        counterResults.CuExecCount[s]     += (upper[0] << 32);
        counterResults.CuExecCycles[s]    += (upper[1] << 32);
        counterResults.CuMinExecCycles[s] += (upper[2] << 32);
        counterResults.CuMaxExecCycles[s] += (upper[3] << 32);

        if (mLogStream.is_open()) {
          mLogStream << "Accelerator Monitor Upper 32, slot " << s << std::endl;
          mLogStream << "  CuExecCount : " << upper[0] << std::endl;
          mLogStream << "  CuExecCycles : " << upper[1] << std::endl;
          mLogStream << "  CuMinExecCycles : " << upper[2] << std::endl;
          mLogStream << "  CuMaxExecCycles : " << upper[3] << std::endl;
        }
      }

      if (hasDataflow) {
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAM_BUSY_CYCLES_OFFSET,
                        &counterResults.CuBusyCycles[s], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAM_MAX_PARALLEL_ITER_OFFSET,
                        &counterResults.CuMaxParallelIter[s], 4);
        if (has64bit) {
          size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAM_BUSY_CYCLES_UPPER_OFFSET,
                        &upper[4], 4);
          size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                        baseAddress + XAM_MAX_PARALLEL_ITER_UPPER_OFFSET,
                        &upper[5], 4);
          counterResults.CuBusyCycles[s]      += (upper[4] << 32);
          counterResults.CuMaxParallelIter[s] += (upper[5] << 32);
        }
      } else {
        counterResults.CuBusyCycles[s] = counterResults.CuExecCycles[s];
        counterResults.CuMaxParallelIter[s] = 1;
      }

      if (mLogStream.is_open()) {
        mLogStream << "Reading Accelerator Monitor... SlotNum : " << s << std::endl;
        mLogStream << "Reading Accelerator Monitor... CuExecCount : " << counterResults.CuExecCount[s] << std::endl;
        mLogStream << "Reading Accelerator Monitor... CuExecCycles : " << counterResults.CuExecCycles[s] << std::endl;
        mLogStream << "Reading Accelerator Monitor... CuMinExecCycles : " << counterResults.CuMinExecCycles[s] << std::endl;
        mLogStream << "Reading Accelerator Monitor... CuMaxExecCycles : " << counterResults.CuMaxExecCycles[s] << std::endl;
        mLogStream << "Reading Accelerator Monitor... CuBusyCycles : " << counterResults.CuBusyCycles[s] << std::endl;
        mLogStream << "Reading Accelerator Monitor... CuMaxParallelIter : " << counterResults.CuMaxParallelIter[s] << std::endl;
      }

      // Check Stall bit
      if (hasStall) {
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAM_ACCEL_STALL_INT_OFFSET,
                      &counterResults.CuStallIntCycles[s], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAM_ACCEL_STALL_STR_OFFSET,
                      &counterResults.CuStallStrCycles[s], 4);
        size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XAM_ACCEL_STALL_EXT_OFFSET,
                      &counterResults.CuStallExtCycles[s], 4);
        if (mLogStream.is_open()) {
          mLogStream << "Stall Counters enabled : " << std::endl;
          mLogStream << "Reading Accelerator Monitor... CuStallIntCycles : " << counterResults.CuStallIntCycles[s] << std::endl;
          mLogStream << "Reading Accelerator Monitor... CuStallStrCycles : " << counterResults.CuStallStrCycles[s] << std::endl;
          mLogStream << "Reading Accelerator Monitor... CuStallExtCycles : " << counterResults.CuStallExtCycles[s] << std::endl;
        }
      }
    }
    /*
     * Read Axi Stream Monitor Data
     */
    if (mLogStream.is_open()) {
        mLogStream << "Reading AXI Stream Monitors.." << std::endl;
    }
    numSlots = getPerfMonNumberSlots(XCL_PERF_MON_STR);
    for (uint32_t s=0; s < numSlots; s++) {
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_STR,s);
      // Sample Register
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XASM_SAMPLE_OFFSET,
                      &sampleInterval, 4);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XASM_NUM_TRANX_OFFSET,
                      &counterResults.StrNumTranx[s], 8);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XASM_DATA_BYTES_OFFSET,
                      &counterResults.StrDataBytes[s], 8);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XASM_BUSY_CYCLES_OFFSET,
                      &counterResults.StrBusyCycles[s], 8);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XASM_STALL_CYCLES_OFFSET,
                      &counterResults.StrStallCycles[s], 8);
      size += xclRead(XCL_ADDR_SPACE_DEVICE_PERFMON,
                      baseAddress + XASM_STARVE_CYCLES_OFFSET,
                      &counterResults.StrStarveCycles[s], 8);
      // AXIS without TLAST is assumed to be one long transfer
      if (counterResults.StrNumTranx[s] == 0 && counterResults.StrDataBytes[s] > 0) {
        counterResults.StrNumTranx[s] = 1;
      }
      if (mLogStream.is_open()) {
        mLogStream << "Reading AXI Stream Monitor... SlotNum : " << s << std::endl;
        mLogStream << "Reading AXI Stream Monitor... NumTranx : " << counterResults.StrNumTranx[s] << std::endl;
        mLogStream << "Reading AXI Stream Monitor... DataBytes : " << counterResults.StrDataBytes[s] << std::endl;
        mLogStream << "Reading AXI Stream Monitor... BusyCycles : " << counterResults.StrBusyCycles[s] << std::endl;
        mLogStream << "Reading AXI Stream Monitor... StallCycles : " << counterResults.StrStallCycles[s] << std::endl;
        mLogStream << "Reading AXI Stream Monitor... StarveCycles : " << counterResults.StrStarveCycles[s] << std::endl;
      }
    }

    return size;
  }

  // *****
  // Trace
  // *****

  // Clock training used in converting device trace timestamps to host domain
  size_t AwsXcl::xclPerfMonClockTraining(xclPerfMonType type) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", "
        << type << ", Send clock training..." << std::endl;
    }
    // This will be enabled later. We're snapping first event to start of cu.
    return 1;
  }

  // Start trace performance monitoring
  size_t AwsXcl::xclPerfMonStartTrace(xclPerfMonType type, uint32_t startTrigger) {
    // StartTrigger Bits:
    // Bit 0: Trace Coarse/Fine     Bit 1: Transfer Trace Ctrl
    // Bit 2: CU Trace Ctrl         Bit 3: INT Trace Ctrl
    // Bit 4: Str Trace Ctrl        Bit 5: Ext Trace Ctrl
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
        << ", " << type << ", " << startTrigger
        << ", Start device tracing..." << std::endl;
    }

    // Update addresses for debug/profile IP
    readDebugIpLayout();

    if (!mIsDeviceProfiling)
      return 0;

    size_t size = 0;
    uint32_t regValue;
    uint64_t baseAddress;
    uint32_t numSlots = getPerfMonNumberSlots(XCL_PERF_MON_MEMORY);

    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_MEMORY,i);
      // Set SPM trace ctrl register bits
      regValue = startTrigger & XAIM_TRACE_CTRL_MASK;
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAIM_TRACE_CTRL_OFFSET, &regValue, 4);
    }

    numSlots = getPerfMonNumberSlots(XCL_PERF_MON_ACCEL);
    for (uint32_t i=0; i < numSlots; i++) {
      baseAddress = getPerfMonBaseAddress(XCL_PERF_MON_ACCEL,i);
      // Set Stall trace control register bits
      // Bit 1 : CU (Always ON)  Bit 2 : INT  Bit 3 : STR  Bit 4 : Ext 
      regValue = ((startTrigger & XAM_TRACE_STALL_SELECT_MASK) >> 1) | 0x1 ;
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress + XAM_TRACE_CTRL_OFFSET, &regValue, 4);
    }

    xclPerfMonGetTraceCount(type);  // XCL_PERF_MON_MEMORY or XCL_PERF_MON_ACCEL should be fine for now
    size += resetFifos(type);
    xclPerfMonGetTraceCount(type);

    for (uint32_t i = 0; i < 2; i++) {
      baseAddress = getTraceFunnelAddress(XCL_PERF_MON_MEMORY);
      uint64_t timeStamp = getHostTraceTimeNsec();
      regValue = static_cast <uint32_t> (timeStamp & 0xFFFF);
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress, &regValue, 4);
      regValue = static_cast <uint32_t> (timeStamp >> 16 & 0xFFFF);
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress, &regValue, 4);
      regValue = static_cast <uint32_t> (timeStamp >> 32 & 0xFFFF); 
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress, &regValue, 4);
      regValue = static_cast <uint32_t> (timeStamp >> 48 & 0xFFFF);
      size += xclWrite(XCL_ADDR_SPACE_DEVICE_PERFMON, baseAddress, &regValue, 4);
      usleep(10);
    }

    return size;
  }

  // Stop trace performance monitoring
  size_t AwsXcl::xclPerfMonStopTrace(xclPerfMonType type) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id() << ", "
        << type << ", Stop and reset device tracing..." << std::endl;
    }

    if (!mIsDeviceProfiling)
      return 0;

    size_t size = 0;
    xclPerfMonGetTraceCount(type);
    size += resetFifos(type);
    return size;
  }

  // Get trace word count
  uint32_t AwsXcl::xclPerfMonGetTraceCount(xclPerfMonType type) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
        << ", " << type << std::endl;
    }

    if (!mIsDeviceProfiling)
      return 0;

    xclAddressSpace addressSpace = (type == XCL_PERF_MON_ACCEL) ?
      XCL_ADDR_KERNEL_CTRL : XCL_ADDR_SPACE_DEVICE_PERFMON;

    uint32_t fifoCount = 0;
    uint32_t numSamples = 0;
    uint32_t numBytes = 0;
    xclRead(addressSpace, getPerfMonFifoBaseAddress(type, 0) + AXI_FIFO_RLR, &fifoCount, 4);
    // Read bits 22:0 per AXI-Stream FIFO product guide (PG080, 10/1/14)
    numBytes = fifoCount & 0x7FFFFF;
    numSamples = numBytes / (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 8);

    if (mLogStream.is_open()) {
      mLogStream << "  No. of trace samples = " << std::dec << numSamples
        << " (fifoCount = 0x" << std::hex << fifoCount << ")" << std::dec << std::endl;
    }

    return numSamples;
  }

  // Read all values from APM trace AXI stream FIFOs
  size_t AwsXcl::xclPerfMonReadTrace(xclPerfMonType type, xclTraceResultsVector& traceVector) {
    if (mLogStream.is_open()) {
      mLogStream << __func__ << ", " << std::this_thread::get_id()
        << ", " << type << ", " << &traceVector
        << ", Reading device trace stream..." << std::endl;
    }

    traceVector.mLength = 0;
    if (!mIsDeviceProfiling)
      return 0;

    uint32_t numSamples = xclPerfMonGetTraceCount(type);
    if (numSamples == 0)
      return 0;

    uint64_t fifoReadAddress[] = { 0, 0, 0 };
    if (type == XCL_PERF_MON_MEMORY) {
      fifoReadAddress[0] = getPerfMonFifoReadBaseAddress(type, 0) + AXI_FIFO_RDFD_AXI_FULL;
    }
    else {
      for (int i = 0; i < 3; i++)
        fifoReadAddress[i] = getPerfMonFifoReadBaseAddress(type, i) + AXI_FIFO_RDFD;
    }

    size_t size = 0;

    // Limit to max number of samples so we don't overrun trace buffer on host
    uint32_t maxSamples = getPerfMonNumberSamples(type);
    numSamples = (numSamples > maxSamples) ? maxSamples : numSamples;
    traceVector.mLength = numSamples;

    const uint32_t bytesPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 8);
    const uint32_t wordsPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 32);
    //uint32_t numBytes = numSamples * bytesPerSample;
    uint32_t numWords = numSamples * wordsPerSample;

    // Create trace buffer on host (requires alignment)
    const int BUFFER_BYTES = MAX_TRACE_NUMBER_SAMPLES * bytesPerSample;
    const int BUFFER_WORDS = MAX_TRACE_NUMBER_SAMPLES * wordsPerSample;
#ifndef _WINDOWS
// TODO: Windows build support
//    alignas is defined in c++11
#if GCC_VERSION >= 40800
    alignas(AXI_FIFO_RDFD_AXI_FULL) uint32_t hostbuf[BUFFER_WORDS];
#else
    xrt_core::AlignedAllocator<uint32_t> alignedBuffer(AXI_FIFO_RDFD_AXI_FULL, BUFFER_WORDS);
    uint32_t* hostbuf = alignedBuffer.getBuffer();
#endif
#else
    uint32_t hostbuf[BUFFER_WORDS];
#endif

    // ******************************
    // Read all words from trace FIFO
    // ******************************
    if (type == XCL_PERF_MON_MEMORY) {

      memset((void *)hostbuf, 0, BUFFER_BYTES);

      // Iterate over chunks
      // NOTE: AXI limits this to 4K bytes per transfer
      uint32_t chunkSizeWords = 256 * wordsPerSample;
      if (chunkSizeWords > 1024) chunkSizeWords = 1024;
      uint32_t chunkSizeBytes = 4 * chunkSizeWords;
      uint32_t words = 0;

      // Read trace a chunk of bytes at a time
      if (numWords > chunkSizeWords) {
        for (; words < (numWords - chunkSizeWords); words += chunkSizeWords) {
          if (mLogStream.is_open()) {
            mLogStream << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
              << std::hex << fifoReadAddress[0] << " and writing it to 0x"
              << (void *)(hostbuf + words) << std::dec << std::endl;
          }

          if (awsbwhal::unmgdPread(mUserHandle, (void *)(hostbuf + words), chunkSizeBytes, fifoReadAddress[0]) < 0)
            return 0;

          size += chunkSizeBytes;
        }
      }

      // Read remainder of trace not divisible by chunk size
      if (words < numWords) {
        chunkSizeBytes = 4 * (numWords - words);

        if (mLogStream.is_open()) {
          mLogStream << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
            << std::hex << fifoReadAddress[0] << " and writing it to 0x"
            << (void *)(hostbuf + words) << std::dec << std::endl;
        }

        if (awsbwhal::unmgdPread(mUserHandle, (void *)(hostbuf + words), chunkSizeBytes, fifoReadAddress[0]) < 0)
          return 0;

        size += chunkSizeBytes;
      }

      if (mLogStream.is_open()) {
        mLogStream << __func__ << ": done reading " << size << " bytes " << std::endl;
      }
    }

    // ******************************
    // Read & process all trace FIFOs
    // ******************************
    static unsigned long long firstTimestamp;
    xclTraceResults results = {};
    uint64_t previousTimestamp = 0;
    for (uint32_t wordnum = 0; wordnum < numSamples; wordnum++) {
      uint32_t index = wordsPerSample * wordnum;
      uint64_t temp = 0;

      temp = *(hostbuf + index) | (uint64_t)*(hostbuf + index + 1) << 32;
      if (!temp)
        continue;

      if (wordnum == 0)
        firstTimestamp = temp & 0x1FFFFFFFFFFF;

      // This section assumes that we write 8 timestamp packets in startTrace
      int mod = (wordnum % 4);
      unsigned int clockWordIndex = 7;
      if (wordnum > clockWordIndex || mod == 0) {
        memset(&results, 0, sizeof(xclTraceResults));
      }
      if (wordnum <= clockWordIndex) {
        if (mod == 0) {
          uint64_t currentTimestamp = temp & 0x1FFFFFFFFFFF;
          if (currentTimestamp >= firstTimestamp)
            results.Timestamp = currentTimestamp - firstTimestamp;
          else 
            results.Timestamp = currentTimestamp + (0x1FFFFFFFFFFF - firstTimestamp);
        }    
        uint64_t partial = (((temp >> 45) & 0xFFFF) << (16 * mod));
        results.HostTimestamp = results.HostTimestamp | partial;
        if (mLogStream.is_open()) {
          mLogStream << "Updated partial host timestamp : " << std::hex << partial << std::endl;
        }    
        if (mod == 3) { 
          if (mLogStream.is_open()) {
            mLogStream << "  Trace sample " << std::dec << wordnum << ": ";
            mLogStream << " Timestamp : " << results.Timestamp << "   ";
            mLogStream << " Host Timestamp : " << std::hex << results.HostTimestamp << std::endl;
          }
          results.isClockTrain = true;
          traceVector.mArray[static_cast<int>(wordnum/4)] = results;
        }    
        continue;
      }

      // Zynq Packet Format
//      results.Timestamp = temp & 0x1FFFFFFFFFFF;
//      results.EventType = ((temp >> 45) & 0xF) ? XCL_PERF_MON_END_EVENT :
//        XCL_PERF_MON_START_EVENT;
      results.Timestamp = (temp & 0x1FFFFFFFFFFF) - firstTimestamp;
      results.EventType = ((temp >> 45) & 0xF) ? XCL_PERF_MON_END_EVENT : 
          XCL_PERF_MON_START_EVENT;

      results.TraceID = (temp >> 49) & 0xFFF;
      results.Reserved = (temp >> 61) & 0x1;
      results.Overflow = (temp >> 62) & 0x1;
      results.Error = (temp >> 63) & 0x1;
      results.EventID = XCL_PERF_MON_HW_EVENT;
      results.EventFlags = ((temp >> 45) & 0xF) | ((temp >> 57) & 0x10) ;
      results.isClockTrain = false;
//      traceVector.mArray[wordnum] = results;
      traceVector.mArray[wordnum - clockWordIndex + 1] = results;

      if (mLogStream.is_open()) {
        mLogStream << "  Trace sample " << std::dec << wordnum << ": ";
//        mLogStream << dec2bin(uint32_t(temp >> 32)) << " " << dec2bin(uint32_t(temp & 0xFFFFFFFF));
        mLogStream << dec2bin(uint32_t(temp>>32)) << " " << dec2bin(uint32_t(temp&0xFFFFFFFF));
        mLogStream << std::endl;
        mLogStream << " Timestamp : " << results.Timestamp << "   ";
        mLogStream << "Event Type : " << results.EventType << "   ";
        mLogStream << "slotID : " << results.TraceID << "   ";
        mLogStream << "Start, Stop : " << static_cast<int>(results.Reserved) << "   ";
        mLogStream << "Overflow : " << static_cast<int>(results.Overflow) << "   ";
        mLogStream << "Error : " << static_cast<int>(results.Error) << "   ";
        mLogStream << "EventFlags : " << static_cast<int>(results.EventFlags) << "   ";
        mLogStream << "Interval : " << results.Timestamp - previousTimestamp << "   ";
        mLogStream << std::endl;
        previousTimestamp = results.Timestamp;
      }
    }

    return size;
  } // end xclPerfMonReadTrace

} // namespace awsbwhal

void xclPerfMonConfigureDataflow(xclDeviceHandle handle, xclPerfMonType type, unsigned *ip_config)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  if (!drv)
    return;
  return drv->xclPerfMonConfigureDataflow(type, ip_config);
}

size_t xclPerfMonStartCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclPerfMonStartCounters(type) : -ENODEV;
}


size_t xclPerfMonStopCounters(xclDeviceHandle handle, xclPerfMonType type)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclPerfMonStopCounters(type) : -ENODEV;
}


size_t xclPerfMonReadCounters(xclDeviceHandle handle, xclPerfMonType type, xclCounterResults& counterResults)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclPerfMonReadCounters(type, counterResults) : -ENODEV;
}


size_t xclPerfMonClockTraining(xclDeviceHandle handle, xclPerfMonType type)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclPerfMonClockTraining(type) : -ENODEV;
}


size_t xclPerfMonStartTrace(xclDeviceHandle handle, xclPerfMonType type, uint32_t startTrigger)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclPerfMonStartTrace(type, startTrigger) : -ENODEV;
}


size_t xclPerfMonStopTrace(xclDeviceHandle handle, xclPerfMonType type)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclPerfMonStopTrace(type) : -ENODEV;
}


uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, xclPerfMonType type)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclPerfMonGetTraceCount(type) : -ENODEV;
}


size_t xclPerfMonReadTrace(xclDeviceHandle handle, xclPerfMonType type, xclTraceResultsVector& traceVector)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclPerfMonReadTrace(type, traceVector) : -ENODEV;
}


double xclGetDeviceClockFreqMHz(xclDeviceHandle handle)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclGetDeviceClockFreqMHz() : 0.0;
}


double xclGetReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclGetReadMaxBandwidthMBps() : 0.0;
}


double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->xclGetWriteMaxBandwidthMBps() : 0.0;
}


size_t xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  return 0;
}


void xclSetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type, uint32_t numSlots)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  if (!drv)
    return;
  return drv->xclSetProfilingNumberSlots(type, numSlots);
}


uint32_t xclGetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->getPerfMonNumberSlots(type) : 2;
}

uint32_t xclGetProfilingSlotProperties(xclDeviceHandle handle, xclPerfMonType type, uint32_t slotnum)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  return drv ? drv->getPerfMonProperties(type, slotnum) : 0;
}

void xclGetProfilingSlotName(xclDeviceHandle handle, xclPerfMonType type, uint32_t slotnum,
		                     char* slotName, uint32_t length)
{
  awsbwhal::AwsXcl *drv = awsbwhal::AwsXcl::handleCheck(handle);
  if (!drv)
    return;
  return drv->getPerfMonSlotName(type, slotnum, slotName, length);
}


void xclWriteHostEvent(xclDeviceHandle handle, xclPerfMonEventType type,
                       xclPerfMonEventID id)
{
  // don't do anything
}


