/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include "xdp/profile/plugin/aie/aie_plugin.h"
#include "xdp/profile/writer/aie/aie_writer.h"

#include "core/common/system.h"
#include "core/common/time.h"
#include "core/common/config_reader.h"
#include "core/include/experimental/xrt-next.h"
#include "core/edge/user/shim.h"
#include "core/edge/common/aie_parser.h"
#include "xdp/profile/database/database.h"

extern "C" {
#include <xaiengine.h>
}

namespace xdp {

  AIEProfilingPlugin::AIEProfilingPlugin() 
      : XDPPlugin()
  {
    db->registerPlugin(this);

    // Get polling interval (in msec)
    mPollingInterval = xrt_core::config::get_aie_profile_interval_ms();
  }

  AIEProfilingPlugin::~AIEProfilingPlugin()
  {
    // Stop the polling thread
    endPoll();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }

      db->unregisterPlugin(this);
    }
  }

  void AIEProfilingPlugin::pollAIECounters(uint32_t index, void* handle)
  {
    auto drv = ZYNQ::shim::handleCheck(handle);
    if (!drv)
      return;
    auto it = thread_ctrl_map.find(handle);
    if (it == thread_ctrl_map.end())
      return;

    auto& should_continue = it->second;
    while (should_continue) {
      // Wait until xclbin has been loaded and device has been updated in database
      if (!(db->getStaticInfo().isDeviceReady(index)))
        continue;
      auto aieArray = drv->getAieArray();
      if (!aieArray)
        continue;

      // Iterate over all AIE Counters
      auto numCounters = db->getStaticInfo().getNumAIECounter(index);
      for (uint64_t c=0; c < numCounters; c++) {
        auto aie = db->getStaticInfo().getAIECounter(index, c);
        if (!aie)
          continue;

        std::vector<uint64_t> values;
        values.push_back(aie->column);
        values.push_back(aie->row);
        values.push_back(aie->startEvent);
        values.push_back(aie->endEvent);
        values.push_back(aie->resetEvent);

        // Read counter value from device
        XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row+1);
        uint32_t counterValue;
        XAie_PerfCounterGet(aieArray->getDevInst(), tileLocation, XAIE_CORE_MOD, aie->counterNumber, &counterValue);
        values.push_back(counterValue);

        // Get timestamp in milliseconds
        double timestamp = xrt_core::time_ns() / 1.0e6;
        db->getDynamicInfo().addAIESample(index, timestamp, values);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(mPollingInterval));     
    }
  }

  void AIEProfilingPlugin::updateAIEDevice(void* handle)
  {
    char pathBuf[512];
    memset(pathBuf, 0, 512);
    xclGetDebugIPlayoutPath(handle, pathBuf, 512);

    std::string sysfspath(pathBuf);
    uint64_t deviceId = db->addDevice(sysfspath); // Get the unique device Id

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

    // Open the writer for this device
    struct xclDeviceInfo2 info;
    xclGetDeviceInfo2(handle, &info);
    std::string deviceName = std::string(info.mName);
    // Create and register writer and file
    std::string outputFile = "aie_profile_" + deviceName + ".csv";
    writers.push_back(new AIEProfilingWriter(outputFile.c_str(),
        deviceName.c_str(), mIndex));
    db->getStaticInfo().addOpenedFile(outputFile.c_str(), "AIE_PROFILE");

    // Start the AIE profiling thread
    thread_ctrl_map[handle] = true;
    auto device_thread = std::thread(&AIEProfilingPlugin::pollAIECounters, this, mIndex, handle);
    thread_map[handle] = std::move(device_thread);

    ++mIndex;
  }

  void AIEProfilingPlugin::endPollforDevice(void* handle)
  {
    // Ask thread to stop
    thread_ctrl_map[handle] = false;

    auto it = thread_map.find(handle);
    if (it != thread_map.end()) {
      it->second.join();
      thread_map.erase(it);
      thread_ctrl_map.erase(handle);
    }
  }

  void AIEProfilingPlugin::endPoll()
  {
    // Ask all threads to end
    for (auto& p : thread_ctrl_map)
      p.second = false;

    for (auto& t : thread_map)
      t.second.join();

    thread_ctrl_map.clear();
    thread_map.clear();
  }

} // end namespace xdp
