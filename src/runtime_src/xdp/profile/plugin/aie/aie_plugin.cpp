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
//#include "core/edge/user/aie/aie.h"
extern "C" {
#include <xaiengine.h>
}

namespace xdp {

  AIEProfilingPlugin::AIEProfilingPlugin() 
      : XDPPlugin(), mKeepPolling(true)
  {
    db->registerPlugin(this);
   
    // Just like HAL and power profiling, go through devices 
    //  that exist and open a file for each
    uint64_t index = 0;
    void* handle = xclOpen(index, "/dev/null", XCL_INFO);
    while (handle != nullptr) {
      // Keep device locally
      auto device = xrt_core::get_userpf_device(handle);
      mDevices.push_back(device);

      // Determine the name of the device
      struct xclDeviceInfo2 info;
      xclGetDeviceInfo2(handle, &info);
      std::string deviceName = std::string(info.mName);

      // Create and register writer and file
      std::string outputFile = "aie_profile_" + deviceName + ".csv"; 
      writers.push_back(new AIEProfilingWriter(outputFile.c_str(),
			    deviceName.c_str(), index));
      db->getStaticInfo().addOpenedFile(outputFile.c_str(), "AIE_PROFILE");

      // Move on to next device
      xclClose(handle);
      ++index;
      handle = xclOpen(index, "/dev/null", XCL_INFO);
    }

    // Get polling interval (in msec)
    mPollingInterval = xrt_core::config::get_aie_profile_interval_ms();

    // Start the AIE profiling thread
    mPollingThread = std::thread(&AIEProfilingPlugin::pollAIECounters, this);
  }

  AIEProfilingPlugin::~AIEProfilingPlugin()
  {
    // Stop the polling thread
    mKeepPolling = false;
    mPollingThread.join();

    if (VPDatabase::alive()) {
      for (auto w : writers) {
        w->write(false);
      }

      db->unregisterPlugin(this);
    }
  }

  void AIEProfilingPlugin::pollAIECounters()
  {
    while (mKeepPolling) {
      uint64_t index = 0;

      // Iterate over all devices
      for (auto device : mDevices) {
        // Wait until xclbin has been loaded and device has been updated in database
        if (!(db->getStaticInfo().isDeviceReady(index))) {
          ++index;
          continue;
        }

        auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());
        if (!drv)
          continue;
        auto aieArray = drv->getAieArray();

        // Iterate over all AIE Counters
        auto numCounters = db->getStaticInfo().getNumAIECounter(index);
        for (uint64_t c=0; c < numCounters; c++) {
          auto aie = db->getStaticInfo().getAIECounter(index, c);

          std::vector<uint64_t> values;
          values.push_back(aie->column);
          values.push_back(aie->row);
          values.push_back(aie->startEvent);
          values.push_back(aie->endEvent);
          values.push_back(aie->resetEvent);

          // Read counter value from device
          XAie_LocType tileLocation = XAie_TileLoc(aie->column, aie->row);
          uint32_t counterValue;
          XAie_PerfCounterGet(aieArray->getDevInst(), tileLocation, XAIE_CORE_MOD, aie->counterNumber, &counterValue);
          values.push_back(counterValue);

          // Get timestamp in milliseconds
          double timestamp = xrt_core::time_ns() / 1.0e6;

	        db->getDynamicInfo().addAIESample(index, timestamp, values);
        }
        ++index;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(mPollingInterval));     
    }
  }

} // end namespace xdp
