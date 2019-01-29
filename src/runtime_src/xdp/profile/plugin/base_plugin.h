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
      XDPPluginI();
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
      typedef std::tuple<std::string, std::string, std::string, std::string, uint32_t> CUPortArgsBankType;
      std::vector<CUPortArgsBankType> CUPortVector;

    public:
      virtual std::vector<CUPortArgsBankType> getCUPortVector() const {return CUPortVector;}

    // *****************
    // Guidance metadata
    // *****************
    public:
      typedef std::map<std::string, std::string> GuidanceMap;
      typedef std::map<std::string, uint32_t> GuidanceMap2;

      enum e_guidance {
        DEVICE_EXEC_TIME,
        CU_CALLS,
        MEMORY_BIT_WIDTH,
        MIGRATE_MEM,
        DDR_BANKS,
        PORT_BIT_WIDTH,
        KERNEL_COUNT,
        OBJECTS_RELEASED
      };

    public:
      virtual void getGuidanceMetadata(RTProfile *profile);
      static void getGuidanceName(e_guidance check, std::string& name);
      // Objects released
      void setObjectsReleased(bool objectsReleased) {IsObjectsReleased = objectsReleased;}
      bool isObjectsReleased() {return IsObjectsReleased;}
      inline GuidanceMap& getDeviceExecTimesMap() {return mDeviceExecTimesMap;}
      inline GuidanceMap& getComputeUnitCallsMap() {return mComputeUnitCallsMap;}
      inline GuidanceMap2& getKernelCountsMap() {return mKernelCountsMap;}

    protected:
      GuidanceMap  mDeviceExecTimesMap;
      GuidanceMap  mComputeUnitCallsMap;
      GuidanceMap2 mKernelCountsMap;
      bool IsObjectsReleased = false;

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
      virtual size_t getDeviceTimestamp(std::string& deviceName) = 0;
      virtual double getReadMaxBandwidthMBps() = 0 ;
      virtual double getWriteMaxBandwidthMBps() = 0;
      // HAL APIS
      virtual unsigned getProfileNumberSlots(xclPerfMonType type,
                                             std::string& deviceName) = 0;
      virtual void getProfileSlotName(xclPerfMonType type,
                                      std::string& deviceName,
                                      unsigned slotnum, std::string& slotName) = 0;
      virtual unsigned getProfileSlotProperties(xclPerfMonType type,
                                                std::string& deviceName,
                                                unsigned slotnum) = 0;

    protected:
      std::map<std::string, std::string> mComputeUnitKernelTraceMap;
      std::map<std::string, unsigned int> mDeviceKernelClockFreqMap;
      xdp::RTUtil::e_flow_mode FlowMode = xdp::RTUtil::CPU;
      std::string mTraceFooterString;

    public:
      inline xdp::RTUtil::e_flow_mode getFlowMode() { return FlowMode; }
      inline void setFlowMode(xdp::RTUtil::e_flow_mode mode) { FlowMode = mode;}
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
