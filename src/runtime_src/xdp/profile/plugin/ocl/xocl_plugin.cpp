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

#include "xocl_plugin.h"
#include "xdp/profile/writer/base_profile.h"
#include "xdp/profile/core/rt_profile.h"

#include "xrt/util/time.h"

namespace xdp {
  // XOCL XDP Plugin constructor
  XoclPlugin::XoclPlugin(xocl::platform* Platform)
  {
    mPlatformHandle = Platform;
  }

  XoclPlugin::~XoclPlugin()
  {
  }

  // **********
  // Trace time
  // **********
  double XoclPlugin::getTraceTime()
  {
    // Get trace time from XRT
    auto nsec = xrt::time_ns();
    return getTimestampMsec(nsec);
  }

  // *************************
  // Accelerator port metadata
  // *************************

  // Find arguments and memory resources for each accel port on given device
  void XoclPlugin::setArgumentsBank(const std::string& deviceName)
  {
    const std::string numerical("0123456789");

    for (auto device_id : mPlatformHandle->get_device_range()) {
      std::string currDevice = device_id->get_unique_name();
      XDP_LOG("setArgumentsBank: current device = %s, # CUs = %d\n",
              currDevice.c_str(), device_id->get_num_cus());
      if (currDevice.find(deviceName) == std::string::npos)
        continue;

      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto currCU = cu->get_name();
        auto currSymbol = cu->get_symbol();

        // Compile set of ports on this CU
        std::set<std::string> portSet;
        for (auto arg : currSymbol->arguments) {
          if ((arg.address_qualifier == 0)
              || (arg.atype != xocl::xclbin::symbol::arg::argtype::indexed))
            continue;

          auto portName = arg.port;
          std::transform(portName.begin(), portName.end(), portName.begin(), ::tolower);
          portSet.insert(portName);
        }

        // Now find all arguments for each port
        for (auto portName : portSet) {
          XDPPluginI::CUPortArgsBankType row;
          std::get<0>(row) = currCU;
          std::get<1>(row) = portName;

          bool firstArg = true;
          std::string memoryName;

          for (auto arg : currSymbol->arguments) {
            auto currPort = arg.port;
            std::transform(currPort.begin(), currPort.end(), currPort.begin(), ::tolower);

            // Address_Qualifier = 1 : AXI MM Port
            // Address_Qualifier = 4 : AXI Stream Port
            if ((currPort == portName) && (arg.address_qualifier == 1 || arg.address_qualifier == 4)
                && (arg.atype == xocl::xclbin::symbol::arg::argtype::indexed)) {
              std::get<2>(row) += (firstArg) ? arg.name : ("|" + arg.name);

              auto portWidth = arg.port_width;
              unsigned long index = (unsigned long)std::stoi(arg.id);

              try {
                auto memidx_mask = cu->get_memidx(index);
                // auto memidx = 0;
                for (unsigned int memidx=0; memidx<memidx_mask.size(); ++memidx) {
                  if (memidx_mask.test(memidx)) {
                    // Get bank tag string from index
                    memoryName = "DDR";
                    if (device_id->is_active())
                      memoryName = device_id->get_xclbin().memidx_to_banktag(memidx);

                    XDP_LOG("setArgumentsBank: idx = %d, memory = %s\n", memidx, memoryName.c_str());
                    break;
                  }
                }
              }
              catch (const std::runtime_error& ex) {
                memoryName = "DDR";
                XDP_LOG("setArgumentsBank: caught error, using default of %s\n", memoryName.c_str());
              }

              // Catch old bank format and report as DDR
              //std::string memoryName2 = memoryName.substr(0, memoryName.find_last_of("["));
              if (memoryName.find("bank0") != std::string::npos)
                memoryName = "DDR[0]";
              else if (memoryName.find("bank1") != std::string::npos)
                memoryName = "DDR[1]";
              else if (memoryName.find("bank2") != std::string::npos)
                memoryName = "DDR[2]";
              else if (memoryName.find("bank3") != std::string::npos)
                memoryName = "DDR[3]";

              std::get<3>(row) = memoryName;
              std::get<4>(row) = portWidth;
              firstArg = false;
            }
          }

          XDP_LOG("setArgumentsBank: %s/%s, args = %s, memory = %s, width = %d\n",
              std::get<0>(row).c_str(), std::get<1>(row).c_str(), std::get<2>(row).c_str(),
              std::get<3>(row).c_str(), std::get<4>(row));
          CUPortVector.push_back(row);
        }

        portSet.clear();
      } // for cu
    } // for device_id
  }

  // Get the arguments and memory resource for a given device/CU/port
  void XoclPlugin::getArgumentsBank(const std::string& deviceName, const std::string& cuName,
   	                                const std::string& portName, std::string& argNames,
   				                    std::string& memoryName)
  {
    argNames = "All";
    memoryName = "DDR";

    bool foundMemory = false;
    std::string portNameCheck = portName;

    size_t index = portName.find_last_of(PORT_MEM_SEP);
    if (index != std::string::npos) {
      foundMemory = true;
      portNameCheck = portName.substr(0, index);
      memoryName = portName.substr(index+1);
    }
    std::transform(portNameCheck.begin(), portNameCheck.end(), portNameCheck.begin(), ::tolower);

    // Find CU and port, then capture arguments and bank
    for (auto& row : CUPortVector) {
      std::string currCU   = std::get<0>(row);
      std::string currPort = std::get<1>(row);

      if ((currCU == cuName) && (currPort == portNameCheck)) {
        argNames = std::get<2>(row);
        // If already found, replace it; otherwise, use it
        if (foundMemory)
          std::get<3>(row) = memoryName;
        else
          memoryName = std::get<3>(row);
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
  }

  void XoclPlugin::getDeviceExecutionTimes(RTProfile *profile)
  {
    // Traverse all devices in platform
    for (auto device_id : mPlatformHandle->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();

      // Get execution time for this device
      // NOTE: if unused, then this returns 0.0
      double deviceExecTime = profile->getTotalKernelExecutionTime(deviceName);
      mDeviceExecTimesMap[deviceName] = std::to_string(deviceExecTime);
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

  void XoclPlugin::getKernelCounts(RTProfile *profile)
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
    auto iter = mComputeUnitKernelTraceMap.find(cuName);
    if (iter != mComputeUnitKernelTraceMap.end()) {
      traceString = iter->second;
    }
    else {
      // CR 1003380 - Runtime does not send all CU Names so we create a key
      std::string kernelName;
      getProfileKernelName(deviceName, cuName, kernelName);
      for (const auto &pair : mComputeUnitKernelTraceMap) {
        auto fullName = pair.second;
        auto first_index = fullName.find_first_of("|");
        auto second_index = fullName.find('|', first_index+1);
        auto third_index = fullName.find('|', second_index+1);
        auto fourth_index = fullName.find("|", third_index+1);
        auto fifth_index = fullName.find("|", fourth_index+1);
        auto sixth_index = fullName.find_last_of("|");
        std::string currKernelName = fullName.substr(third_index + 1, fourth_index - third_index - 1);
        if (currKernelName == kernelName) {
          traceString = fullName.substr(0,fifth_index + 1) + cuName + fullName.substr(sixth_index);
          return;
        }
      }
      traceString = std::string();
    }
  }

  void XoclPlugin::setTraceStringForComputeUnit(const std::string& cuName, std::string& traceString)
  {
    mComputeUnitKernelTraceMap[cuName] = traceString;
  }

  size_t XoclPlugin::getDeviceTimestamp(std::string& deviceName)
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

  unsigned XoclPlugin::getProfileNumberSlots(xclPerfMonType type, std::string& deviceName)
  {
    unsigned numSlots = xoclp::platform::get_profile_num_slots(mPlatformHandle,
        deviceName, type);
    return numSlots;
  }

  void XoclPlugin::getProfileSlotName(xclPerfMonType type, std::string& deviceName,
                                       unsigned slotnum, std::string& slotName)
  {
    xoclp::platform::get_profile_slot_name(mPlatformHandle, deviceName,
        type, slotnum, slotName);
  }

  unsigned XoclPlugin::getProfileSlotProperties(xclPerfMonType type, std::string& deviceName,
                                                unsigned slotnum)
  {
    return xoclp::platform::get_profile_slot_properties(mPlatformHandle, deviceName, type, slotnum);
  }

  void XoclPlugin::sendMessage(const std::string &msg)
  {
    xrt::message::send(xrt::message::severity_level::WARNING, msg);
  }
} // xdp
