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

#include "xdp/profile/plugin/power/power_plugin.h"
#include "xdp/profile/writer/power/power_writer.h"
#include "core/common/system.h"
#include "core/common/time.h"
#include "core/common/config_reader.h"
#include "core/include/experimental/xrt-next.h"

namespace xdp {

  const char* PowerProfilingPlugin::powerFiles[] = 
    {
      "xmc_12v_aux_curr",
      "xmc_12v_aux_vol",
      "xmc_12v_pex_curr",
      "xmc_12v_pex_vol",
      "xmc_vccint_curr",
      "xmc_vccint_vol",
      "xmc_3v3_pex_curr",
      "xmc_3v3_pex_vol",
      "xmc_cage_temp0",
      "xmc_cage_temp1",
      "xmc_cage_temp2",
      "xmc_cage_temp3",
      "xmc_dimm_temp0",
      "xmc_dimm_temp1",
      "xmc_dimm_temp2",
      "xmc_dimm_temp3",
      "xmc_fan_temp",
      "xmc_fpga_temp",
      "xmc_hbm_temp",
      "xmc_se98_temp0",
      "xmc_se98_temp1",
      "xmc_se98_temp2",
      "xmc_vccint_temp",
      "xmc_fan_rpm"      
    } ;

  PowerProfilingPlugin::PowerProfilingPlugin() :
    XDPPlugin(), keepPolling(true), pollingInterval(20)
  {
    db->registerPlugin(this) ;

    pollingInterval = xrt_core::config::get_power_profile_interval_ms() ;
   
    // Just like HAL level device profiling, go through the devices 
    //  that exist in order to find all of the sysfs paths
    uint64_t index = 0 ;
    void* handle = xclOpen(index, "/dev/null", XCL_INFO) ;
    while (handle != nullptr)
    {
      // For each device, keep track of the paths to the sysfs files
      std::vector<std::string> paths ;
      for (auto f : powerFiles)
      {
	char sysfsPath[512] ;
	xclGetSysfsPath(handle, "xmc", f, sysfsPath, 512) ;
	paths.push_back(sysfsPath) ;
      }
      filePaths.push_back(paths) ;

      // Determine the name of the device
      struct xclDeviceInfo2 info ;
      xclGetDeviceInfo2(handle, &info) ;
      std::string deviceName = std::string(info.mName) ;
      std::string outputFile = "power_profile_" + deviceName + ".csv" ; 

      writers.push_back(new PowerProfilingWriter(outputFile.c_str(),
						 deviceName.c_str(),
					         index)) ;
      (db->getStaticInfo()).addOpenedFile(outputFile.c_str(), 
					  "XRT_POWER_PROFILE") ;

      // Move on to the next device
      xclClose(handle) ;
      ++index ;
      handle = xclOpen(index, "/dev/null", XCL_INFO) ;
    }

    // Start the power profiling thread
    pollingThread = std::thread(&PowerProfilingPlugin::pollPower, this) ;
  }

  PowerProfilingPlugin::~PowerProfilingPlugin()
  {
    // Stop the polling thread
    keepPolling = false ;
    pollingThread.join() ;

    if (VPDatabase::alive())
    {
      for (auto w : writers)
      {
	w->write(false) ;
      }

      db->unregisterPlugin(this) ;
    }
  }

  void PowerProfilingPlugin::pollPower()
  {
    while(keepPolling)
    {
      // Get timestamp in milliseconds
      double timestamp = xrt_core::time_ns() / 1.0e6 ;
      uint64_t index = 0 ;
      for (auto device : filePaths)
      {
	std::vector<uint64_t> values ;
	for (auto file : device)
	{
	  std::ifstream fs(file) ;
	  if (!fs)
	  {
	    // When we tried to get the path to this file, we got a bad
	    //  result (like empty string).  So all devices are aligned and
	    //  have the same amount of information we'll just record this
	    //  data element as 0.
	    values.push_back(0) ;
	    continue ;
	  }
	  std::string data ;
	  std::getline(fs, data) ;
	  uint64_t dp = data.empty() ? 0 : std::stoul(data) ;
	  values.push_back(dp) ;
	  fs.close() ;
	}
	(db->getDynamicInfo()).addPowerSample(index, timestamp, values) ;
	++index ;	
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(pollingInterval)) ;
    }
  }

} // end namespace xdp
