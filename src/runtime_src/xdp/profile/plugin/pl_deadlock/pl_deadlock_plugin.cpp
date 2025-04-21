/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_PLUGIN_SOURCE

#include <array>
#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include "core/common/message.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/api/ip_int.h"

#include "pl_deadlock_plugin.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/pl_constructs.h"
#include "xdp/profile/database/static_info/xclbin_info.h"
#include "xdp/profile/device/pl_device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/utility.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/pl_deadlock/pl_deadlock.h"

#include "core/common/system.h"
#include "core/common/message.h"

namespace xdp {

  constexpr uint32_t REGSIZE_BYTES = 0x4;

  using severity_level = xrt_core::message::severity_level;

  PLDeadlockPlugin::PLDeadlockPlugin()
  : XDPPlugin()
  , mFileExists(false)
  {
    db->registerPlugin(this);
  }

  PLDeadlockPlugin::~PLDeadlockPlugin()
  {
    if (VPDatabase::alive()) {
      writeAll(false);
      db->unregisterPlugin(this);
    }
  }

  void PLDeadlockPlugin::writeAll(bool /*openNewFiles*/)
  {
    // Ask all threads to end
    for (auto& p : mThreadCtrlMap)
      p.second = false;

    for (auto& t : mThreadMap)
      t.second.join();

    mThreadCtrlMap.clear();
    mThreadMap.clear();
  }


  void PLDeadlockPlugin::pollDeadlock(void *hwCtxImpl, uint64_t deviceId)
  {
    auto it = mThreadCtrlMap.find(deviceId);
    if (it == mThreadCtrlMap.end())
      return;

    auto& should_continue = it->second;

    PLDeviceIntf* deviceIntf = (db->getStaticInfo()).getDeviceIntf(deviceId);
    if (nullptr == deviceIntf)
      return;

    xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(hwCtxImpl);

    while (should_continue) {
      if (deviceIntf->getDeadlockStatus()) {
        std::string deviceName = (db->getStaticInfo()).getDeviceName(deviceId);
        std::string msg = "System Deadlock detected on device " + deviceName +
                          ". Please manually terminate and debug the application.";
        xrt_core::message::send(severity_level::warning, "XRT", msg);

        std::string deadlockInfo = deviceName + " :\n";
        std::string allCUDiagnosis;

        ConfigInfo* currConfig = db->getStaticInfo().getCurrentlyLoadedConfig(deviceId);
        if (!currConfig)
          return;
        
        XclbinInfo* currXclbin = currConfig->getPlXclbin();
        if (!currXclbin)
          return;

        for (const auto& cu : currXclbin->pl.cus) {
          std::string cuInstFullName = cu.second->getFullname();
          std::string kernelName = cuInstFullName.substr(0, cuInstFullName.find(':'));

          KernelRegisterInfo kernelRegInfo;
          for (auto& pair : mIpMetadata->kernel_infos) {
            if (kernelName.compare(pair.first) != 0) {
              continue;
            }

            std::unique_ptr<xrt::ip> xrtIP = std::make_unique<xrt::ip>(hwContext, cuInstFullName);
            uint32_t low  = pair.second.begin()->first;
            uint32_t high = pair.second.rbegin()->first + REGSIZE_BYTES;
            xrt_core::ip_int::set_read_range(*xrtIP, low, (high-low));

            std::string currCUDiagnosis;
            for (const auto& entry: pair.second) { 
              auto offset = entry.first;
              auto& messages = entry.second;
              uint32_t kernelInstRegData = xrtIP->read_register(offset);
              for (unsigned int i=0; i < NUM_DEADLOCK_DIAGNOSIS_BITS; i++) {
                if ((kernelInstRegData >> i) & 0x1) {
                  currCUDiagnosis += messages[i] + "\n";
                }
              }
            }
            xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", currCUDiagnosis);
            allCUDiagnosis += currCUDiagnosis;
          }
        }
        if (!allCUDiagnosis.empty()) {
          deadlockInfo += allCUDiagnosis;
          db->getDynamicInfo().setPLDeadlockInfo(deviceId, deadlockInfo);
          // There is only one file for all the devices
          // In case of a deadlock, the application is hung
          // So, we have to write this data ASAP
          forceWrite();
        }
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(mPollingIntervalMs));
    }
  }

  void PLDeadlockPlugin::forceWrite()
  {
    std::lock_guard<std::mutex> lock(mWriteLock);
    std::string outputFile = "pl_deadlock_diagnosis.txt";
    if (!mFileExists) {
      db->getStaticInfo().addOpenedFile(outputFile, "PL_DEADLOCK_DIAGNOSIS");
      mFileExists = true;
    }
    // Don't allocate memory because this application is
    // potentially hung and could be killed
    PlDeadlockWriter(outputFile.c_str()).write(false);
  }

  void PLDeadlockPlugin::flushDevice(void* hwCtxImpl)
  {
    uint64_t deviceId = mHwCtxImplToDevIdMap[hwCtxImpl];

    mThreadCtrlMap[deviceId] = false;
    auto it = mThreadMap.find(deviceId);
    if (it != mThreadMap.end()) {
      it->second.join();
      mThreadMap.erase(it);
      mThreadCtrlMap.erase(deviceId);
    }
    mHwCtxImplToDevIdMap.erase(hwCtxImpl);
  }

  void PLDeadlockPlugin::updateDevice(void* hwCtxImpl)
  {
    xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(hwCtxImpl);
    auto coreDevice = xrt_core::hw_context_int::get_core_device(hwContext);

    auto handle = coreDevice->get_device_handle();

    uint64_t deviceId = db->addDevice(util::getDebugIpLayoutPath(handle));
    mHwCtxImplToDevIdMap[hwCtxImpl] = deviceId;

    auto it = mThreadMap.find(deviceId);
    if (it != mThreadMap.end()) {
      return;
    }

    if (!(db->getStaticInfo()).isDeviceReady(deviceId)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDeviceFromHandle(deviceId,std::move(std::make_unique<HalDevice>(handle)), handle);
    }

    PLDeviceIntf* deviceIntf = (db->getStaticInfo()).getDeviceIntf(deviceId);

    if (!deviceIntf->hasDeadlockDetector()) {
      std::string deviceName = (db->getStaticInfo()).getDeviceName(deviceId);
      std::string msg = "System Deadlock Detector not found on device " + deviceName;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
      return;
    }

    mIpMetadata = db->getStaticInfo().populateIpMetadata(deviceId, coreDevice);

    // Start the PL deadlock detection thread
    mThreadCtrlMap[deviceId] = true;
    mThreadMap[deviceId] = std::thread { [=] { pollDeadlock(hwCtxImpl, deviceId); } };
  }

} // end namespace xdp
