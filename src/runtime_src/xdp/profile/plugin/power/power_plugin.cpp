/**
 * Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include <map>
#include <string>

#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/system.h"
#include "core/common/time.h"
#include "core/include/xrt/experimental/xrt-next.h"
#include "core/common/query_requests.h"
#include "core/include/xrt/xrt_device.h"

#include "xdp/profile/plugin/power/power_plugin.h"
#include "xdp/profile/writer/power/power_writer.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/device/utility.h"

namespace xdp {

  PowerProfilingPlugin::PowerProfilingPlugin() :
    XDPPlugin(), keepPolling(true), pollingInterval(20)
  {
    db->registerPlugin(this) ;
    db->registerInfo(info::power) ;

    pollingInterval = xrt_core::config::get_power_profile_interval_ms() ;

    // There can be multiple boards with the same shell loaded as well as
    //  different boards.  We number them all individually.
    std::map<std::string, uint64_t> deviceNumbering ;

   uint32_t numDevices = xrt_core::get_total_devices(true).second;
   uint32_t index = 0;
   while (index < numDevices) {
     try {
       xrtDevices.push_back(std::make_unique<xrt::device>(index));
       auto ownedHandle = xrtDevices[index]->get_handle()->get_device_handle();

        // Determine the name of the device
        std::string deviceName = util::getDeviceName(ownedHandle);
  
        if (deviceNumbering.find(deviceName) == deviceNumbering.end()) {
          deviceNumbering[deviceName] = 0 ;
        }
        deviceName += "-" ;
        deviceName += std::to_string(deviceNumbering[deviceName]) ;
        deviceNumbering[deviceName]++ ;

        std::string outputFile = "power_profile_" + deviceName + ".csv" ; 
  
        VPWriter* writer = new PowerProfilingWriter(outputFile.c_str(),
                                                    deviceName.c_str(),
                                                    index) ;
        writers.push_back(writer) ;
        (db->getStaticInfo()).addOpenedFile(writer->getcurrentFileName(), 
                                          "XRT_POWER_PROFILE") ;

        // Move on to the next device
        ++index;
      } catch (const std::runtime_error& e) {
        std::string msg = "Could not open device at index " + std::to_string(index) + e.what();
        xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", msg);
        ++index;
        continue;
      }  
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

      for(auto& xrtDevice : xrtDevices)
      {
        std::vector<uint64_t> values ;
        std::shared_ptr<xrt_core::device> coreDevice = xrtDevice->get_handle();
        
        if (!coreDevice) {
          ++index;
          continue;
        }

        try{
          uint64_t data = 0;
          data = xrt_core::device_query<xrt_core::query::v12v_aux_milliamps>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::v12v_aux_millivolts>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::v12v_pex_milliamps>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::v12v_pex_millivolts>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::int_vcc_milliamps>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::int_vcc_millivolts>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::v3v3_pex_milliamps>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::v3v3_pex_millivolts>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::cage_temp_0>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::cage_temp_1>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::cage_temp_2>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::cage_temp_3>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::dimm_temp_0>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::dimm_temp_1>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::dimm_temp_2>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::dimm_temp_3>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::fan_trigger_critical_temp>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::temp_fpga>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::hbm_temp>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::temp_card_top_front>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::temp_card_top_rear>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::temp_card_bottom_front>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::int_vcc_temp>(coreDevice);
          values.push_back(data);
          data = xrt_core::device_query<xrt_core::query::fan_speed_rpm>(coreDevice); 
          values.push_back(data);
        }
        catch (const xrt_core::query::no_such_key&) {
          //query is not implemented
        }
        catch (const std::exception&) {
          // error retrieving information
          std::string msg = "Error while retrieving data from power files. Using default value.";
          xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg);
        }
        (db->getDynamicInfo()).addPowerSample(index, timestamp, values) ;
        ++index ;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(pollingInterval)) ;
    }
  }

} // end namespace xdp
