/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include "xocl_plugin.h"
#include "xdp/profile/writer/base_profile.h"
#include "xdp/profile/core/rt_profile.h"
#include <cctype> // needed for std::tolower

#ifdef _WIN32
#pragma warning (disable : 4244)
/* Disable warning for "int" to "char" conversion in <algorithm> header file included in one of the included files */
#endif

namespace xdp {
  // XOCL XDP Plugin constructor
  XoclPlugin::XoclPlugin(xocl::platform* Platform)
  {
    mPlatformHandle = Platform;

    /*
     * Gather Static info at init
     * as it might not be safe at the end
     */
    getXrtIniSettings();

  }

  XoclPlugin::~XoclPlugin()
  {
  }

  // **********
  // Trace time
  // **********
  double XoclPlugin::getTraceTime()
  {
    // Everything in xocl layer should use this API
    auto nsec = xocl::time_ns();
    return getTimestampMsec(nsec);
  }

  // *************************
  // Accelerator port metadata
  // *************************

  // Get the name of the memory resource associated with a device, CU, and memory index
  // NOTE: This is used for comparison purposes to group associated arguments, hence we
  // use the resource name. The actual reporting (see getArgumentsBank below) may include
  // the indices as well, as taken from debug_ip_layout.
  void XoclPlugin::getMemoryNameFromID(const xocl::device* device_id, const std::shared_ptr<xocl::compute_unit> cu,
                                       const std::string arg_id, std::string& memoryName)
  {
    try {
      unsigned long index = (unsigned long)std::stoi(arg_id);
      auto memidx_mask = cu->get_memidx(index);
      // auto memidx = 0;
      for (unsigned int memidx=0; memidx<memidx_mask.size(); ++memidx) {
        if (memidx_mask.test(memidx)) {
          // Get bank tag string from index
          memoryName = "DDR";
          if (device_id->is_active())
            memoryName = device_id->get_xclbin().memidx_to_banktag(memidx);

          XDP_LOG("getMemoryNameFromID: idx = %d, memory = %s\n", memidx, memoryName.c_str());
          break;
        }
      }
    }
    catch (const std::runtime_error& ) {
      memoryName = "DDR";
      XDP_LOG("getMemoryNameFromID: caught error, using default of %s\n", memoryName.c_str());
    }

    // Catch old bank format and report as DDR
    if (memoryName.find("bank") != std::string::npos)
      memoryName = "DDR";

    // Only return the resource name (i.e., remove indices)
    memoryName = memoryName.substr(0, memoryName.find_last_of("["));
  }

  // Find arguments and memory resources for each accel port on given device
  void XoclPlugin::setArgumentsBank(const std::string& deviceName)
  {
    // Iterate over all devices in platform
    for (auto device_id : mPlatformHandle->get_device_range()) {
      std::string currDevice = device_id->get_unique_name();
      XDP_LOG("setArgumentsBank: current device = %s, # CUs = %d\n",
              currDevice.c_str(), device_id->get_num_cus());
      if (currDevice.find(deviceName) == std::string::npos)
        continue;

      // Iterate over all CUs on this device
      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto currCU = cu->get_name();
        auto currSymbol = cu->get_symbol();

        // Compile sets of ports and memories for this CU
        std::set<std::string> portSet;
        std::set<std::string> memorySet;
        for (auto arg : currSymbol->arguments) {
          if ((arg.address_qualifier == 0)
              || (arg.atype != xocl::xclbin::symbol::arg::argtype::indexed))
            continue;

          auto portName = arg.port;
          // Avoid conflict with boost
          // std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);
          std::transform(portName.begin(), portName.end(), portName.begin(), [](char c){return (char) std::tolower(c);});
          portSet.insert(portName);

          std::string memoryName;
          getMemoryNameFromID(device_id, cu, arg.id, memoryName);
          memorySet.insert(memoryName);
        }

        // Now find all arguments for each port/memory resource pair
        for (auto portName : portSet) {
          for (auto memoryName : memorySet) {
            XDPPluginI::CUPortArgsBankType row;
            std::get<0>(row) = currCU;
            std::get<1>(row) = portName;
            std::get<3>(row) = memoryName;

            bool foundArg = false;

            for (auto arg : currSymbol->arguments) {
              // Catch arguments we don't care about
              //   Address_Qualifier = 1 : AXI MM Port
              //   Address_Qualifier = 4 : AXI Stream Port
              if (((arg.address_qualifier != 1) && (arg.address_qualifier != 4))
                  || (arg.atype != xocl::xclbin::symbol::arg::argtype::indexed))
                continue;

              auto currPort = arg.port;
              // Avoid conflict with boost
              // std::transform(currPort.begin(), currPort.end(), currPort.begin(), ::tolower);
              std::transform(currPort.begin(), currPort.end(), currPort.begin(), [](char c){return (char) std::tolower(c);});

              std::string currMemory;
              getMemoryNameFromID(device_id, cu, arg.id, currMemory);

              if ((currPort == portName) && (currMemory == memoryName)) {
                std::get<2>(row) += (!foundArg) ? arg.name : ("|" + arg.name);
                std::get<4>(row) = arg.port_width;
                foundArg = true;
              }
            } // for args

            if (foundArg) {
              XDP_LOG("setArgumentsBank: %s/%s, args = %s, memory = %s, width = %d\n",
                std::get<0>(row).c_str(), std::get<1>(row).c_str(), std::get<2>(row).c_str(),
                std::get<3>(row).c_str(), std::get<4>(row));
              CUPortVector.push_back(row);
            }
          } // for memory 
        } // for port

        portSet.clear();
        memorySet.clear();
      } // for CU
    } // for device
  }

  // Get the arguments and memory resource for a given device/CU/port
  void XoclPlugin::getArgumentsBank(const std::string& /*deviceName*/, const std::string& cuName,
   	                                const std::string& portName, std::string& argNames,
                                    std::string& memoryName)
  {
    argNames = "All";
    memoryName = "DDR";

    bool foundMemory = false;
    std::string portNameCheck = portName;
    std::string memoryResource = memoryName;

    // Given a port string (e.g., "cu1/port1-DDR[0]"), separate out the port name 
    // and the memory resource name (e.g., "DDR") 
    size_t index = portName.find_last_of(IP_LAYOUT_SEP);
    if (index != std::string::npos) {
      foundMemory = true;
      portNameCheck = portName.substr(0, index);
      memoryName = portName.substr(index+1);

      size_t index2 = memoryName.find("[");
      memoryResource = memoryName.substr(0, index2);
    }
    // Avoid conflict with boost
    //std::transform(portNameCheck.begin(), portNameCheck.end(), portNameCheck.begin(), ::tolower);
    std::transform(portNameCheck.begin(), portNameCheck.end(), portNameCheck.begin(), [](char c){return (char) std::tolower(c);});

    // Find CU and port, then capture arguments and bank
    for (auto& row : CUPortVector) {
      std::string currCU   = std::get<0>(row);
      std::string currPort = std::get<1>(row);

      if ((currCU == cuName) && (currPort == portNameCheck)) {
        std::string currMemory = std::get<3>(row);
        size_t index3 = currMemory.find("[");
        auto currMemoryResource = currMemory.substr(0, index3);

        // Make sure it's the right memory resource
        if (foundMemory && (currMemoryResource != memoryResource))
          continue;

        argNames = std::get<2>(row);
        memoryName = currMemory;
        break;
      }
    }
  }

  // *****************
  // Guidance metadata
  // *****************

  // Gather statistics and put into param/value map
  // NOTE: this needs to be called while the platforms and devices still exist
  void XoclPlugin::getGuidanceMetadata(RTProfile *profile)
  {
    // 1. Device execution times (and unused devices)
    getDeviceExecutionTimes(profile);

    // 2. Unused CUs
    getUnusedComputeUnits(profile);

    // 3. Kernel counts
    getKernelCounts(profile);

    // 4. Devices with PLRAM Size > 0
    getPlramSizeDevices();

    // 5. Bit widths for memory types for each device
    getMemBitWidthDevices();

    // 6. Memory Bank Info from Mem Topology
    getMemUsageStats();
  }

  void XoclPlugin::getDeviceExecutionTimes(RTProfile *profile)
  {
    // NOTE: all device are assumed to support PLRAMs
    setPlramDevice(true);
    setHbmDevice(false);
    setKdmaDevice(false);
    setP2PDevice(false);

    // Total kernel time for entire application = (last end - first start)
    double totalKernelTimeMsec = profile->getTotalApplicationKernelTimeMsec();
    setTotalApplicationKernelTimeMs(totalKernelTimeMsec);

    // Traverse all devices in platform
    for (auto device_id : mPlatformHandle->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();

      // Get execution time for this device
      // NOTE: if unused, then this returns 0.0
      double deviceExecTime = profile->getTotalKernelExecutionTime(deviceName);
      mDeviceExecTimesMap[deviceName] = std::to_string(deviceExecTime);

      // TODO: checks below are kludgy; are there better ways to check for device support?

      // Check if device supports HBM
      if (deviceName.find("u280") != std::string::npos ||
          deviceName.find("u50") != std::string::npos) {
        setHbmDevice(true);
      }

      // Check if device supports M2M
      if ((deviceName.find("xilinx_u200_xdma_201830_2") != std::string::npos)
          || (deviceName.find("xilinx_u200_xdma_201830_3") != std::string::npos)
          || (deviceName.find("xilinx_vcu1525_xdma_201830_2") != std::string::npos))
        setKdmaDevice(true);

      // Check if device supports P2P
      if ((deviceName.find("xilinx_u200_xdma_201830_2") != std::string::npos)
          || (deviceName.find("xilinx_u200_xdma_201830_3") != std::string::npos)
          || (deviceName.find("xilinx_u250_xdma_201830_2") != std::string::npos)
          || (deviceName.find("xilinx_vcu1525_xdma_201830_2") != std::string::npos)
          || (deviceName.find("samsung") != std::string::npos))
        setP2PDevice(true);
    }
  }

  void XoclPlugin::getUnusedComputeUnits(RTProfile *profile)
  {
    // Traverse all devices in platform
    for (auto device_id : mPlatformHandle->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();

      // Traverse all CUs on current device
      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto cuName = cu->get_name();

        // Get number of calls for current CU
        int numCalls = profile->getComputeUnitCalls(deviceName, cuName);
        std::string cuFullName = deviceName + "|" + cuName;
        mComputeUnitCallsMap[cuFullName] = std::to_string(numCalls);
      }
    }
  }

  void XoclPlugin::getKernelCounts(RTProfile* /*profile*/)
  {
    // Traverse all devices in this platform
    for (auto device_id : mPlatformHandle->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();

      // Traverse all CUs on current device
      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto kernelName = cu->get_kernel_name();

        if (mKernelCountsMap.find(kernelName) == mKernelCountsMap.end())
          mKernelCountsMap[kernelName] = 1;
        else
          mKernelCountsMap[kernelName] += 1;
      }
    }
  }

  void XoclPlugin::getPlramSizeDevices()
  {
    for (auto device : mPlatformHandle->get_device_range()) {
      if (!device->is_active())
        continue;
      auto name = device->get_unique_name();
      auto sz = xdp::xoclp::platform::device::getPlramSizeBytes(device);
      if (sz)
        mDevicePlramSizeMap[name] = sz;
    }
  }

  void XoclPlugin::getMemUsageStats()
  {
    for (auto device : mPlatformHandle->get_device_range()) {
      if (!device->is_active())
        continue;
      xdp::xoclp::platform::device::getMemUsageStats(device, mDeviceMemUsageStatsMap);
    }
  }

  void XoclPlugin::getMemBitWidthDevices()
  {
    for (auto device : mPlatformHandle->get_device_range()) {
      if (!device->is_active())
        continue;

      // TODO: Find a better way to distinguish embedded platforms
      bool soc = false;
      std::string deviceName = device->get_unique_name();
      if (deviceName.rfind("zc", 0) == 0) {
        soc = true;
      }

      // TODO: figure out how to get this from platform
      auto name = device->get_unique_name();
      if (soc) {
        mDeviceMemTypeBitWidthMap[name + "|DDR"] = 64;
      } else {
        mDeviceMemTypeBitWidthMap[name + "|HBM"] = 256;
        mDeviceMemTypeBitWidthMap[name + "|DDR"] = 512;
        mDeviceMemTypeBitWidthMap[name + "|PLRAM"] = 512;
      }
    }
  }

  void XoclPlugin::getXrtIniSettings()
  {
    mXrtIniMap["profile"] = std::to_string(xrt_xocl::config::get_profile());
    mXrtIniMap["timeline_trace"] = std::to_string(xrt_xocl::config::get_timeline_trace());
    mXrtIniMap["data_transfer_trace"] = xrt_xocl::config::get_data_transfer_trace();
    mXrtIniMap["power_profile"] = std::to_string(xrt_xocl::config::get_power_profile());
    mXrtIniMap["stall_trace"] = xrt_xocl::config::get_stall_trace();
    mXrtIniMap["trace_buffer_size"] = xrt_xocl::config::get_trace_buffer_size();
    mXrtIniMap["aie_trace_buffer_size"] = xrt_xocl::config::get_aie_trace_buffer_size();
    mXrtIniMap["verbosity"] = std::to_string(xrt_xocl::config::get_verbosity());
    mXrtIniMap["continuous_trace"] = std::to_string(xrt_xocl::config::get_continuous_trace());
    mXrtIniMap["continuous_trace_interval_ms"] = std::to_string(xrt_xocl::config::get_continuous_trace_interval_ms());
    mXrtIniMap["lop_trace"] = std::to_string(xrt_xocl::config::get_lop_trace());
    mXrtIniMap["launch_waveform"] = xrt_xocl::config::get_launch_waveform();
  }

  // ****************************************
  // Platform Metadata required by profiler
  // ****************************************

  void XoclPlugin::getProfileKernelName(const std::string& deviceName, const std::string& cuName, std::string& kernelName)
  {
    xoclp::platform::get_profile_kernel_name(mPlatformHandle, deviceName, cuName, kernelName);
  }

  void XoclPlugin::getTraceStringFromComputeUnit(const std::string& deviceName,
      const std::string& cuName, std::string& traceString)
  {
    std::string kernel;
    getProfileKernelName(deviceName, cuName, kernel);
    for (const auto &pair : mComputeUnitKernelTraceMap) {
      if (pair.first == kernel) {
        auto index = pair.second.find_last_of("|");
        traceString = pair.second.substr(0,index + 1) + cuName + pair.second.substr(index);
        return;
      }
    }
    traceString = std::string();
  }

  void XoclPlugin::setTraceStringForComputeUnit(const std::string& cuName, std::string& traceString)
  {
    if (!cuName.empty() && mComputeUnitKernelTraceMap.find(cuName) == mComputeUnitKernelTraceMap.end())
      mComputeUnitKernelTraceMap[cuName] = traceString;
  }

  size_t XoclPlugin::getDeviceTimestamp(const std::string& deviceName)
  {
    return xoclp::platform::get_device_timestamp(mPlatformHandle,deviceName);
  }

  double XoclPlugin::getReadMaxBandwidthMBps()
  {
    return xoclp::platform::get_device_max_read(mPlatformHandle);
  }

  double XoclPlugin::getWriteMaxBandwidthMBps()
  {
    return xoclp::platform::get_device_max_write(mPlatformHandle);
  }

  unsigned int XoclPlugin::getProfileNumberSlots(xclPerfMonType type, const std::string& deviceName)
  {
    unsigned int numSlots = xoclp::platform::get_profile_num_slots(mPlatformHandle,
        deviceName, type);
    return numSlots;
  }

  void XoclPlugin::getProfileSlotName(xclPerfMonType type, const std::string& deviceName,
                                       unsigned int slotnum, std::string& slotName)
  {
    xoclp::platform::get_profile_slot_name(mPlatformHandle, deviceName,
        type, slotnum, slotName);
  }

  void XoclPlugin::getTraceSlotName(xclPerfMonType type, const std::string& deviceName,
                                       unsigned int slotnum, std::string& slotName)
  {
    xoclp::platform::get_trace_slot_name(mPlatformHandle, deviceName,
        type, slotnum, slotName);
  }

  unsigned int XoclPlugin::getProfileSlotProperties(xclPerfMonType type, const std::string& deviceName,
                                                unsigned int slotnum)
  {
    return xoclp::platform::get_profile_slot_properties(mPlatformHandle, deviceName, type, slotnum);
  }

  unsigned int XoclPlugin::getTraceSlotProperties(xclPerfMonType type, const std::string& deviceName,
                                                unsigned int slotnum)
  {
    return xoclp::platform::get_trace_slot_properties(mPlatformHandle, deviceName, type, slotnum);
  }

  bool XoclPlugin::isAPCtrlChain(const std::string& deviceName,
                                     const std::string& cu)
  {
    return xoclp::platform::is_ap_ctrl_chain(mPlatformHandle, deviceName,cu);
  }

  void XoclPlugin::sendMessage(const std::string &msg)
  {
    xrt_xocl::message::send(xrt_xocl::message::severity_level::XRT_WARNING, msg);
  }
} // xdp
