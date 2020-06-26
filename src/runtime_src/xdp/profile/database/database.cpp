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

#include <iostream>

#define XDP_SOURCE

#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  bool VPDatabase::live ;

  VPDatabase::VPDatabase()
            : numDevices(0)
  {
    VPDatabase::live = true ;
  }

  // The database and all the plugins are singletons and can be
  //  destroyed at the end of the execution in any order.  So, 
  //  each plugin is responsible for registering itself at the 
  //  time the library is loaded and removing it if it is destroyed first.
  VPDatabase::~VPDatabase()
  {
    // The only plugins that should still be in this vector are ones
    //  that have not been destroyed yet.
    for (auto p : plugins)
    {
      p->writeAll(false) ;
    }

    plugins.clear();
    devices.clear();
    numDevices = 0;
    VPDatabase::live = false ;    
  }

  VPDatabase* VPDatabase::Instance()
  {
    static VPDatabase db ;
    return &db ;
  }

  bool VPDatabase::alive()
  {
    return VPDatabase::live ;
  }

  uint64_t VPDatabase::getDeviceId(const std::string& sysfsPath)
  {
    if (devices.find(sysfsPath) == devices.end())
    {
      throw std::runtime_error("Device not registered in database");
    }
    return devices[sysfsPath] ;
  }

  uint64_t VPDatabase::addDevice(const std::string& sysfsPath)
  {
    if(devices.find(sysfsPath) == devices.end()) {
      devices[sysfsPath] = numDevices++;
    }
    return devices[sysfsPath];
  }

  // This function should return true the first time any plugin calls it.
  //  The plugin that has ownership is the only one that should be responsible
  //  for writing the run summary.
  bool VPDatabase::claimRunSummaryOwnership()
  {
    static std::mutex runSummaryLock ;
    static bool claimed = false ;

    std::lock_guard<std::mutex> lock(runSummaryLock) ;
    if (claimed)
    {
      return false ;
    }
    claimed = true ;
    return true ;
  }

  // This function should return true the first time any plugin calls it.
  //  The plugin that has ownership is the only one that should be responsible
  //  for offloading information from the devices.  This is necessary for
  //  hardware OpenCL flows which will end up loading two offload plugins
  bool VPDatabase::claimDeviceOffloadOwnership()
  {
    static std::mutex deviceOffloadLock ;
    static bool claimed = false ;

    std::lock_guard<std::mutex> lock(deviceOffloadLock) ;
    if (claimed) return false ;

    claimed = true ;
    return true ;
  }

} // end namespace xdp
