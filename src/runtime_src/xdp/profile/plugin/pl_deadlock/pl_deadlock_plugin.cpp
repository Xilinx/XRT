/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#include <string>
#include <vector>
#include <iostream>

#include "core/common/xrt_profiling.h"
#include "core/common/message.h"
#include "pl_deadlock_plugin.h"

#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include "xdp/profile/device/device_intf.h"
#include "xdp/profile/device/hal_device/xdp_hal_device.h"

#include "core/common/system.h"
#include "core/common/message.h"

namespace xdp {

  using severity_level = xrt_core::message::severity_level;

  PLDeadlockPlugin::PLDeadlockPlugin() : XDPPlugin()
  {
    db->registerPlugin(this);
    mAtomicKeepPolling= true;
  }

  PLDeadlockPlugin::~PLDeadlockPlugin()
  {
    writeAll(false);

    if (VPDatabase::alive())
      db->unregisterPlugin(this);
  }

  void PLDeadlockPlugin::writeAll(bool /*openNewFiles*/)
  {
    // Stop the polling thread
    mAtomicKeepPolling = false;
    for (auto& t: mThreadVector) {
      if (t.joinable())
        t.join();
    }
  }


  void PLDeadlockPlugin::pollDeadlock(uint32_t deviceId)
  {
    std::string deviceName = (db->getStaticInfo()).getDeviceName(deviceId);
    DeviceIntf* deviceIntf = (db->getStaticInfo()).getDeviceIntf(deviceId);

    if (deviceIntf == nullptr)
      return;
    if (!deviceIntf->hasDeadlockDetector())
      return;

    while (mAtomicKeepPolling) {
      if (deviceIntf->getDeadlockStatus()) {
        std::string msg = "System Deadlock detected on device " + deviceName;
        xrt_core::message::send(severity_level::warning, "XRT", msg);
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(mPollingIntervalMs));
    }
  }

  void PLDeadlockPlugin::updateDevice(void* handle)
  {
    const unsigned int PATH_LENGTH = 512;
    char pathBuf[PATH_LENGTH];
    memset(pathBuf, 0, PATH_LENGTH);
    xclGetDebugIPlayoutPath(handle, pathBuf, PATH_LENGTH);
    std::string path(pathBuf);
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
    if (deviceIntf == nullptr) {
      // If DeviceIntf is not already created, create a new one to communicate with physical device
      deviceIntf = new DeviceIntf();
      try {
        deviceIntf->setDevice(new HalDevice(handle));
        deviceIntf->readDebugIPlayout();
      } catch (std::exception& e) {
        // Read debug IP layout could throw an exception
        std::stringstream msg;
        msg << "Unable to read debug IP layout for device " << deviceId << ": " << e.what();
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        delete deviceIntf;
        return;
      }
      (db->getStaticInfo()).setDeviceIntf(deviceId, deviceIntf);
    }

    // Start the PL deadlock detection thread
    mThreadVector.emplace_back(&PLDeadlockPlugin::pollDeadlock, this, deviceId);
  }
  
} // end namespace xdp
