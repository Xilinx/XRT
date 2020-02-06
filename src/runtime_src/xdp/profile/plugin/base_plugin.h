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

#ifndef __XDP_BASE_PLUGIN_H
#define __XDP_BASE_PLUGIN_H

#include <boost/format.hpp>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <chrono>
#include <iostream>
#include <map>

#include "xdp/profile/core/rt_util.h"

// Use this class to build plugins. All XDP plugins should support
// these functions for proper reporting.

namespace xdp {
    class RTProfile;

    // Base plugin class
    class XDPPluginI {

    public:
      XDP_EXPORT
      XDPPluginI();

      XDP_EXPORT
    virtual ~XDPPluginI();

    // **********
    // Trace time
    // **********
    public:
    virtual double getTraceTime();

    // Get timestamp in msec given time in nsec
    double getTimestampMsec(uint64_t timeNsec) {
      return (timeNsec / 1.0e6);
    }

    // *************************
    // Accelerator port metadata
    // *************************
    public:
    // Set the accelerator port information (i.e., fill the CUPortVector)
      virtual void setArgumentsBank(const std::string& deviceName);
    // Get information for a given accelerator/port pair
      virtual void getArgumentsBank(const std::string& deviceName, const std::string& cuName,
                                    const std::string& portName, std::string& argNames,
          std::string& memoryName);

    protected:
      // Tuple of accelerator port information
      // Index  Type      Description
      //   0    string    Name of accelerator or compute unit
      //   1    string    Name of port
      //   2    string    List of kernel arguments (separated by '|')
      //   3    string    Name of memory resource this port is connected to
      //   4    uint32_t  Bit width of this port
      typedef std::tuple<std::string, std::string, std::string, std::string, size_t> CUPortArgsBankType;
      std::vector<CUPortArgsBankType> CUPortVector;

    public:
      virtual std::vector<CUPortArgsBankType> getCUPortVector() const {return CUPortVector;}

    // *****************
    // Guidance metadata
    // *****************
    public:
      typedef std::map<std::string, std::string> GuidanceMap;
      typedef std::map<std::string, uint64_t> GuidanceMap2;
      typedef std::map<uint64_t, uint64_t> GuidanceMap3;
      typedef std::map<uint64_t, std::vector<std::string>> GuidanceMap4;

      enum e_guidance {
        DEVICE_EXEC_TIME,
        CU_CALLS,
        MEMORY_BIT_WIDTH,
        MIGRATE_MEM,
        MEMORY_USAGE,
        PLRAM_DEVICE,
        HBM_DEVICE,
        KDMA_DEVICE,
        P2P_DEVICE,
        P2P_HOST_TRANSFERS,
        PORT_BIT_WIDTH,
        KERNEL_COUNT,
        OBJECTS_RELEASED,
        CU_CONTEXT_EN,
        TRACE_MEMORY,
        MAX_PARALLEL_KERNEL_ENQUEUES,
        COMMAND_QUEUE_OOO,
        PLRAM_SIZE_BYTES,
        KERNEL_BUFFER_INFO,
        TRACE_BUFFER_FULL,
        MEMORY_TYPE_BIT_WIDTH,
        BUFFER_RD_ACTIVE_TIME_MS,
        BUFFER_WR_ACTIVE_TIME_MS,
        BUFFER_TX_ACTIVE_TIME_MS,
        APPLICATION_RUN_TIME_MS
      };

    public:
      virtual void getGuidanceMetadata(RTProfile *profile);

      static void getGuidanceName(e_guidance check, std::string& name);
      // Objects released
      void setObjectsReleased(bool objectsReleased) {IsObjectsReleased = objectsReleased;}
      bool isObjectsReleased() {return IsObjectsReleased;}
      // Device that supports PLRAM
      void setPlramDevice(bool plramDevice) {IsPlramDevice = plramDevice;}
      bool isPlramDevice() {return IsPlramDevice;}
      // Device that supports HBM
      void setHbmDevice(bool hbmDevice) {IsHbmDevice = hbmDevice;}
      bool isHbmDevice() {return IsHbmDevice;}
      // Device that supports KDMA
      void setKdmaDevice(bool kdmaDevice) {IsKdmaDevice = kdmaDevice;}
      bool isKdmaDevice() {return IsKdmaDevice;}
      // Device that supports P2P
      void setP2PDevice(bool p2pDevice) {IsP2PDevice = p2pDevice;}
      bool isP2PDevice() {return IsP2PDevice;}
      // Get maps of metadata results used for guidance
      inline GuidanceMap& getDeviceExecTimesMap() {return mDeviceExecTimesMap;}
      inline GuidanceMap& getComputeUnitCallsMap() {return mComputeUnitCallsMap;}
      inline GuidanceMap2& getKernelCountsMap() {return mKernelCountsMap;}
      inline GuidanceMap2& getKernelMaxParallelStartsMap() {return mKernelMaxParallelStartsMap;}
      inline GuidanceMap2& getDeviceMemTypeBitWidthMap() {return mDeviceMemTypeBitWidthMap;}
      inline GuidanceMap2& getDeviceTraceBufferFullMap() {return mDeviceTraceBufferFullMap;}
      inline GuidanceMap2& getDevicePlramSizeMap() {return mDevicePlramSizeMap;}
      inline GuidanceMap3& getmCQInfoMap() {return mCQInfoMap;}
      inline GuidanceMap4& getKernelBufferInfoMap() {return mKernelBufferInfoMap;}
      // Host Buffer first start to last end
      // Read, Write and Aggregate times
      void logBufferEvent(double timestamp, bool isRead);
      double getRdBufferActiveTimeMs() {return mLastBufferReadMs - mFirstBufferReadMs;}
      double getWrBufferActiveTimeMs() {return mLastBufferWriteMs - mFirstBufferWriteMs;}
      double getBufferActiveTimeMs();
      // Application run time
      void setApplicationEnd() {mApplicationRunTimeMs = getTraceTime();}
      double getApplicationRunTimeMs() {return mApplicationRunTimeMs;}
      //Profiling infrastructure metadata
      void setCtxEn(bool ctxEn) {IsCtxEn = ctxEn;}
      bool isCtxEn() {return IsCtxEn;}
      void setTraceMemory(const std::string& traceMemory) {TraceMemory = traceMemory;}
      std::string getTraceMemory() {return TraceMemory;}

    protected:
      GuidanceMap  mDeviceExecTimesMap;
      GuidanceMap2 mDevicePlramSizeMap;
      GuidanceMap  mComputeUnitCallsMap;
      GuidanceMap2 mKernelCountsMap;
      GuidanceMap2 mKernelMaxParallelStartsMap;
      GuidanceMap2 mDeviceMemTypeBitWidthMap;
      GuidanceMap2 mDeviceTraceBufferFullMap;
      GuidanceMap4 mKernelBufferInfoMap;
      GuidanceMap3 mCQInfoMap;
      bool IsObjectsReleased = false;
      bool IsPlramDevice = false;
      bool IsHbmDevice = false;
      bool IsKdmaDevice = false;
      bool IsP2PDevice = false;
      bool IsCtxEn = false;
      std::string TraceMemory = "NA";
      double mApplicationRunTimeMs = 0.0;
      // Buffer Reads
      double mFirstBufferReadMs = 0.0;
      double mLastBufferReadMs = 0.0;
      // Buffer Writes
      double mFirstBufferWriteMs = 0.0;
      double mLastBufferWriteMs = 0.0;

    // ****************************************
    // Platform Metadata required by profiler
    // ****************************************
    public:
      virtual void getProfileKernelName(const std::string& deviceName,
                                        const std::string& cuName,
                                        std::string& kernelName) = 0;
      virtual void getTraceStringFromComputeUnit(const std::string& deviceName,
                                                 const std::string& cuName,
                                                 std::string& traceString) = 0;
      virtual size_t getDeviceTimestamp(const std::string& deviceName) = 0;
      virtual double getReadMaxBandwidthMBps() = 0 ;
      virtual double getWriteMaxBandwidthMBps() = 0;
      // HAL APIS
      virtual unsigned int getProfileNumberSlots(xclPerfMonType type,
                                            const std::string& deviceName) = 0;
      virtual void getProfileSlotName(xclPerfMonType type,
                                      const std::string& deviceName,
                                      unsigned int slotnum, std::string& slotName) = 0;
      virtual unsigned int getProfileSlotProperties(xclPerfMonType type,
                                                const std::string& deviceName,
                                                unsigned int slotnum) = 0;
      virtual bool isAPCtrlChain(const std::string& deviceName, const std::string& cu) = 0;

    protected:
      std::map<std::string, std::string> mComputeUnitKernelTraceMap;
      std::map<std::string, unsigned int> mDeviceKernelClockFreqMap;
      xdp::RTUtil::e_flow_mode FlowMode = xdp::RTUtil::CPU;
      bool mSystemDPAEmulation = true;
      std::string mTraceFooterString;

    public:
      inline xdp::RTUtil::e_flow_mode getFlowMode() { return FlowMode; }
      inline void setFlowMode(xdp::RTUtil::e_flow_mode mode) { FlowMode = mode;}

      inline bool getSystemDPAEmulation()           { return mSystemDPAEmulation; }
      inline void setSystemDPAEmulation(bool value) { mSystemDPAEmulation = value; }

      inline void setTraceFooterString(std::string traceFooterString) {
        mTraceFooterString = traceFooterString;
      };
      inline void getTraceFooterString(std::string& trString) {
        trString = mTraceFooterString;
      };
      void setKernelClockFreqMHz(const std::string &deviceName, unsigned int clockRateMHz) {
        mDeviceKernelClockFreqMap[deviceName] = clockRateMHz;
      }
      unsigned int getKernelClockFreqMHz(std::string &deviceName) {
        auto iter = mDeviceKernelClockFreqMap.find(deviceName);
        if (iter != mDeviceKernelClockFreqMap.end())
          return iter->second;
        return 300;
      }

      // Lets profiler communicate to application
      public:
        virtual void sendMessage(const std::string &msg);
    };

} // xdp

#endif
