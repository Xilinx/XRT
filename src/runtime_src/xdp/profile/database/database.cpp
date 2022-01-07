/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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

#include "core/common/config_reader.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/writer/vp_base/summary_writer.h"

namespace xdp {

  bool VPDatabase::live ;

  VPDatabase::VPDatabase() :
    stats(this), staticdb(this), dyndb(this), pluginInfo(0), numDevices(0)
  {
    VPDatabase::live = true ;

    summary = new SummaryWriter("summary.csv", this) ;
  }

  // The database and all the plugins are singletons and can be
  //  destroyed at the end of the execution in any order.  So, 
  //  each plugin is responsible for registering itself at the 
  //  time the library is loaded and removing it if it is destroyed first.
  VPDatabase::~VPDatabase()
  {
    // The only plugins that should still be in this vector are ones
    //  that have not been destroyed yet.
    for (auto p : plugins) {
      p->writeAll(false) ;
    }

    // After all the plugins have written their data, we can dump the
    //  generic summary
    if (summary != nullptr) {
      staticdb.addOpenedFile(summary->getcurrentFileName(), "PROFILE_SUMMARY") ;
      summary->write(false) ;
      delete summary ;
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
      throw std::runtime_error("Device not registered in database");

    return devices[sysfsPath] ;
  }

  uint64_t VPDatabase::addDevice(const std::string& sysfsPath)
  {
    if(devices.find(sysfsPath) == devices.end())
      devices[sysfsPath] = numDevices++;

    return devices[sysfsPath];
  }

  void VPDatabase::broadcast(MessageType msg, void* blob)
  {
    for (auto p : plugins) {
      p->broadcast(msg, blob) ;
    }
  }
} // end namespace xdp
