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

#include "xdp/profile/plugin/noc/noc_plugin.h"
#include "xdp/profile/writer/noc/noc_writer.h"
#include "core/common/system.h"
#include "core/common/time.h"
#include "core/include/experimental/xrt-next.h"

namespace xdp {

  NOCProfilingPlugin::NOCProfilingPlugin() 
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

      std::string outputFile = "noc_profile_" + deviceName + ".csv"; 
      writers.push_back(new NOCProfilingWriter(outputFile.c_str(),
			    deviceName.c_str(), index));
      (db->getStaticInfo()).addOpenedFile(outputFile.c_str(), "NOC_PROFILE") ;

      // Move on to next device
      xclClose(handle);
      ++index;
      handle = xclOpen(index, "/dev/null", XCL_INFO);
    }

    // Start the NOC profiling thread
    mPollingThread = std::thread(&NOCProfilingPlugin::pollNOCCounters, this);
  }

  NOCProfilingPlugin::~NOCProfilingPlugin()
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

  void NOCProfilingPlugin::pollNOCCounters()
  {
    uint32_t pollnum = 0;

    while (mKeepPolling) {
      // Get timestamp in milliseconds
      double timestamp = xrt_core::time_ns() / 1.0e6;
      uint64_t index = 0;

      // TODO: not sure what we need from device; for now, just use name
      for (auto device : mDevices) {
        // TODO: traverse all NMUs used in design
        for (uint32_t nmu=0; nmu < 2; ++nmu) {
          std::vector<uint64_t> values;

          // TODO: replace dummy data with counter values

          // Read
          uint64_t readByteCount    = pollnum * 128;
          uint64_t readBurstCount   = pollnum * 10;
          uint64_t readTotalLatency = pollnum * 1000;
          uint64_t readMinLatency   = 42;
          uint64_t readMaxLatency   = 100;
          values.push_back(readByteCount);
          values.push_back(readBurstCount);
          values.push_back(readTotalLatency);
          values.push_back(readMinLatency);
          values.push_back(readMaxLatency);

          // Write
          uint64_t writeByteCount    = pollnum * 128;
          uint64_t writeBurstCount   = pollnum * 10;
          uint64_t writeTotalLatency = pollnum * 1000;
          uint64_t writeMinLatency   = 42;
          uint64_t writeMaxLatency   = 100;
          values.push_back(writeByteCount);
          values.push_back(writeBurstCount);
          values.push_back(writeTotalLatency);
          values.push_back(writeMinLatency);
          values.push_back(writeMaxLatency);

          // TODO: add NMU name to call and database (second column in file)
	        (db->getDynamicInfo()).addNOCSample(index, timestamp, values);
	        ++index;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      ++pollnum;      
    }
  }

} // end namespace xdp
