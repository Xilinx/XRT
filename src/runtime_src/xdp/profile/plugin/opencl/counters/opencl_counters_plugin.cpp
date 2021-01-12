/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include "xocl/core/platform.h"
#include "xocl/core/device.h"

#include "xdp/profile/plugin/opencl/counters/opencl_counters_plugin.h"
#include "xdp/profile/writer/opencl/opencl_summary_writer.h"

#ifdef _WIN32
/* Disable warning for use of std::getenv */
#pragma warning (disable : 4996)
#endif

namespace xdp {

  OpenCLCountersProfilingPlugin::OpenCLCountersProfilingPlugin() : XDPPlugin()
  {
    db->registerPlugin(this) ;

    writers.push_back(new OpenCLSummaryWriter("opencl_summary.csv")) ;
    (db->getStaticInfo()).addOpenedFile("opencl_summary.csv", "PROFILE_SUMMARY") ;

    platform = xocl::get_shared_platform() ;
  }

  OpenCLCountersProfilingPlugin::~OpenCLCountersProfilingPlugin()
  {
    if (VPDatabase::alive())
    {
      // OpenCL could be running hardware emulation or software emulation,
      //  so be sure to account for any peculiarities here.
      emulationSetup() ;

      // Before writing, make sure that counters are read.
      db->broadcast(VPDatabase::READ_COUNTERS, nullptr) ;
      for (auto w : writers)
      {
	w->write(false) ;
      }
      db->unregisterPlugin(this) ;
    }
  }

  void OpenCLCountersProfilingPlugin::emulationSetup()
  {
    XDPPlugin::emulationSetup() 
;
    char* internalsSummary = getenv("VITIS_KERNEL_PROFILE_FILENAME") ;
    if (internalsSummary != nullptr) {
      (db->getStaticInfo()).addOpenedFile(internalsSummary, "KERNEL_PROFILE");
    }
  }

  // This function is only called in hardware emulation.  For hardware
  //  emulation there should only ever be one device.
  uint64_t OpenCLCountersProfilingPlugin::convertToEstimatedTimestamp(uint64_t realTimestamp)
  {
    uint64_t convertedTimestamp = realTimestamp ;

    auto device = platform->get_device_range()[0] ;
    uint64_t deviceTimestamp = device->get_xdevice()->getDeviceTime().get() ;

    if (deviceTimestamp != 0)
      convertedTimestamp = deviceTimestamp ;

    return convertedTimestamp ;
  }

} // end namespace xdp
