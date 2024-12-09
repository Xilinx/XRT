/**
 * Copyright (C) 2020-2021 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include "core/common/system.h"
#include "core/common/time.h"
#include "core/common/config_reader.h"
#include "core/include/xrt/experimental/xrt-next.h"
#include "core/include/xrt/xrt_device.h"

#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/plugin/noc/noc_plugin.h"
#include "xdp/profile/writer/noc/noc_writer.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/device/utility.h"

#include <boost/algorithm/string.hpp>

namespace xdp {

  NOCProfilingPlugin::NOCProfilingPlugin() 
      : XDPPlugin(), mKeepPolling(true)
  {
    db->registerPlugin(this);
    db->registerInfo(info::noc);

    uint32_t numDevices = xrt_core::get_total_devices(true).second;
    uint32_t index = 0;
    while (index < numDevices) {
      try {
        auto xrtDevice = std::make_unique<xrt::device>(index);
        auto ownedHandle = xrtDevice->get_handle()->get_device_handle();
        // Determine the name of the device
        std::string deviceName = util::getDeviceName(ownedHandle);
        mDevices.push_back(deviceName);
  
        std::string outputFile = "noc_profile_" + deviceName + ".csv"; 
        VPWriter* writer = new NOCProfilingWriter(outputFile.c_str(),
                                                  deviceName.c_str(),
                                                  index) ;
        writers.push_back(writer);
        db->getStaticInfo().addOpenedFile(writer->getcurrentFileName(), "NOC_PROFILE") ;
      } catch (const std::runtime_error &) {
        break;
      }
      ++index; 
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
    /*
    uint64_t pollnum = 0;

    while (mKeepPolling) {
      // Get timestamp in milliseconds
      double timestamp = xrt_core::time_ns() / 1.0e6;
      uint64_t index = 0;

      // Iterate over all devices
      for (auto device : mDevices) {
        XclbinInfo* currentXclbin = db->getStaticInfo().getCurrentlyLoadedXclbin(index);
        // Iterate over all NOC NMUs
        auto numNOC = db->getStaticInfo().getNumNOC(index, currentXclbin);
        for (uint64_t n=0; n < numNOC; n++) {
          auto noc = db->getStaticInfo().getNOC(index, currentXclbin, n);

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
	  //      db->getDynamicInfo().addNOCSample(index, timestamp, cellName, values);
	        ++index;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(mPollingInterval));
      ++pollnum;      
    }
    */
  }

} // end namespace xdp
