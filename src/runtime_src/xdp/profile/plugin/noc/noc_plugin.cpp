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
#include "core/common/config_reader.h"
#include "core/include/experimental/xrt-next.h"

#include <boost/algorithm/string.hpp>

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
      db->getStaticInfo().addOpenedFile(outputFile.c_str(), "NOC_PROFILE") ;

      // Move on to next device
      xclClose(handle);
      ++index;
      handle = xclOpen(index, "/dev/null", XCL_INFO);
    }

    // Get polling interval (in msec)
    mPollingInterval = xrt_core::config::get_noc_profile_interval_ms();

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

      // Iterate over all devices
      for (auto device : mDevices) {
        // Iterate over all NOC NMUs
        auto numNOC = db->getStaticInfo().getNumNOC(index);
        for (uint64_t n=0; n < numNOC; n++) {
          auto noc = db->getStaticInfo().getNOC(index, n);

          // Name = <master>-<NMU cell>-<read QoS>-<write QoS>-<NPI freq>-<AIE freq>
          std::vector<std::string> result; 
          boost::split(result, noc->name, boost::is_any_of("-"));
          std::string cellName = (result.size() > 1) ? result[1] : "N/A";

          // TODO: replace dummy data with counter values
          std::vector<uint64_t> values;

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
          uint64_t writeByteCount    = pollnum * 234;
          uint64_t writeBurstCount   = pollnum * 21;
          uint64_t writeTotalLatency = pollnum * 1234;
          uint64_t writeMinLatency   = 24;
          uint64_t writeMaxLatency   = 123;
          values.push_back(writeByteCount);
          values.push_back(writeBurstCount);
          values.push_back(writeTotalLatency);
          values.push_back(writeMinLatency);
          values.push_back(writeMaxLatency);

          // Add sample to dynamic database
	        db->getDynamicInfo().addNOCSample(index, timestamp, cellName, values);
	        ++index;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(mPollingInterval));
      ++pollnum;      
    }
  }

} // end namespace xdp
