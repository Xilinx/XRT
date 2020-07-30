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
#include "core/include/experimental/xrt-next.h"

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
      // Determine the name of the device
      struct xclDeviceInfo2 info;
      xclGetDeviceInfo2(handle, &info);
      std::string deviceName = std::string(info.mName);
      mDevices.push_back(deviceName);

      std::string outputFile = "aie_profile_" + deviceName + ".csv"; 
      writers.push_back(new AIEProfilingWriter(outputFile.c_str(),
			    deviceName.c_str(), index));
      (db->getStaticInfo()).addOpenedFile(outputFile.c_str(), "AIE_PROFILE") ;

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
    uint32_t pollnum = 0;

    while (mKeepPolling) {
      // Get timestamp in milliseconds
      double timestamp = xrt_core::time_ns() / 1.0e6;
      uint64_t index = 0;

      // TODO: not sure what we need from device; for now, just use name
      for (auto device : mDevices) {
        // TODO: traverse all tiles used in design
        for (uint32_t tile=0; tile < 2; ++tile) {
          std::vector<uint64_t> values;
          uint64_t column = 10 * tile;
          uint64_t row = tile;
          values.push_back(column);
          values.push_back(row);

          for (uint32_t c=0; c < NUM_AIE_COUNTERS; ++c) {
            // TODO: for now, just use dummy values
            uint64_t startEvent = (c==0) ? 28 : ((c== 1) ? 22 : 0);
            uint64_t endEvent   = (c==0) ? 29 : ((c== 1) ? 22 : 0);
            uint64_t resetEvent = 0;
            uint64_t value      = (c+1) * pollnum * 100;
            values.push_back(startEvent);
            values.push_back(endEvent);
            values.push_back(resetEvent);
            values.push_back(value);
          }

	        (db->getDynamicInfo()).addAIESample(index, timestamp, values);
	        ++index;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(mPollingInterval));
      ++pollnum;      
    }
  }

} // end namespace xdp
