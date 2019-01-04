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

// TODO: remove this dependency (needed for DeviceData)
#include "xdp/profile/plugin/ocl/xocl_profile.h"

// Use this class to build plugins. All XDP plugins should support
// these functions for proper reporting.

namespace xdp {
    class RTProfile;
    class ProfileWriterI;

    // Base plugin class
    class XDPPluginI {

    public:
      XDPPluginI();
	  virtual ~XDPPluginI() {};

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
      virtual void writeGuidanceMetadataSummary(ProfileWriterI* writer, RTProfile *profile);
      static void getGuidanceName(e_guidance check, std::string& name);

    protected:
      GuidanceMap  mDeviceExecTimesMap;
      GuidanceMap  mComputeUnitCallsMap;
      GuidanceMap2 mKernelCountsMap;

    // ***************
    // Device metadata
    // ***************
    public:
      // Key-data map of device data (moved from rt_profile.h)
      // TODO: move this out of the base plugin class
      std::map<xdp::profile::device::key, xdp::profile::device::data> DeviceData;

    // ****************************************
    // Platform Metadata required by profiler
    // ****************************************
    public:
      virtual void getProfileKernelName(const std::string& deviceName, const std::string& cuName, std::string& kernelName);
      virtual void getTraceStringFromComputeUnit(const std::string& deviceName,
        const std::string& cuName, std::string& traceString);

    protected:
      std::map<std::string, std::string> mComputeUnitKernelTraceMap;
    };

} // xdp

#endif
