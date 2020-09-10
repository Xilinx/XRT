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

#include <cstring>
#include "xdp/profile/plugin/vart/vart_plugin.h"

#define MAX_PATH_SZ 512

namespace xdp {

  VARTPlugin::VARTPlugin() : XDPPlugin()
  {
    db->registerPlugin(this);

    //std::string version = "1.0" ;
    //std::string creationTime = xdp::getCurrentDateTime();
    //std::string xrtVersion   = xdp::getXRTVersion();
    //std::string toolVersion  = xdp::getToolVersion();

    // TODO: Create an appropriate writer here once moved from VART
    //writers.push_back(new VARTTraceWriter("vart_trace.csv",
    //                  version, creationTime, xrtVersion, toolVersion));
    
    (db->getStaticInfo()).addOpenedFile("profile_summary.csv", "PROFILE");
    (db->getStaticInfo()).addOpenedFile("vitis_ai_profile.csv", "VITIS_AI_PROFILE");
    (db->getStaticInfo()).addOpenedFile("vart_trace.csv", "VP_TRACE");
  }

  VARTPlugin::~VARTPlugin()
  {
    if (VPDatabase::alive()) {
      db->unregisterPlugin(this);
    }

    // If the database is dead, then we must have already forced a 
    //  write at the database destructor so we can just move on
  }
}
