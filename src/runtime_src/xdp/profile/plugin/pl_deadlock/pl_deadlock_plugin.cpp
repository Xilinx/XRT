/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "core/common/xrt_profiling.h"
#include "core/common/message.h"
#include "pl_deadlock_plugin.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"
#include "xdp/profile/device/utility.h"

#include "core/common/system.h"
#include "core/common/message.h"

namespace xdp {

  using severity_level = xrt_core::message::severity_level;

  PLDeadlockPlugin::PLDeadlockPlugin() : XDPPlugin()
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


  void PLDeadlockPlugin::pollDeadlock(void* handle, uint64_t deviceId)
  {
    std::string deviceName = (db->getStaticInfo()).getDeviceName(deviceId);
    DeviceIntf* deviceIntf = (db->getStaticInfo()).getDeviceIntf(deviceId);

    if (deviceIntf == nullptr)
      return;
    if (!deviceIntf->hasDeadlockDetector()) {
      std::string msg = "System Deadlock Detector not found on device " + deviceName;
      xrt_core::message::send(severity_level::warning, "XRT", msg);
      return;
    }

    auto it = mThreadCtrlMap.find(handle);
    if (it == mThreadCtrlMap.end())
      return;
    auto& should_continue = it->second;

    while (should_continue) {
      if (deviceIntf->getDeadlockStatus()) {
        std::string msg = "System Deadlock detected on device " + deviceName +
        ". Please manually terminate and debug the application.";
        xrt_core::message::send(severity_level::warning, "XRT", msg);
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(mPollingIntervalMs));
    }
  }

  void PLDeadlockPlugin::flushDevice(void* handle)
  {
    mThreadCtrlMap[handle] = false;
    auto it = mThreadMap.find(handle);
    if (it != mThreadMap.end()) {
      it->second.join();
      mThreadMap.erase(it);
      mThreadCtrlMap.erase(handle);
    }
  }

  void PLDeadlockPlugin::updateDevice(void* handle)
  {
    std::array<char, sysfs_max_path_length> pathBuf = {0};
    xclGetDebugIPlayoutPath(handle, pathBuf.data(), (sysfs_max_path_length-1) ) ;
    std::string path(pathBuf.data());
    uint64_t deviceId = db->addDevice(path);

    if (!(db->getStaticInfo()).isDeviceReady(deviceId)) {
      // Update the static database with information from xclbin
      (db->getStaticInfo()).updateDevice(deviceId, handle);
      {
        struct xclDeviceInfo2 info;
        if(xclGetDeviceInfo2(handle, &info) == 0) {
          (db->getStaticInfo()).setDeviceName(deviceId, std::string(info.mName));
        }
      }
    }

    DeviceIntf* deviceIntf = (db->getStaticInfo()).getDeviceIntf(deviceId);
    if (deviceIntf == nullptr)
      deviceIntf = db->getStaticInfo().createDeviceIntf(deviceId, new HalDevice(handle));

    // Start the PL deadlock detection thread
    mThreadCtrlMap[handle] = true;
    mThreadMap[handle] = std::thread { [=] { pollDeadlock(handle, deviceId); } };
  }

} // end namespace xdp
