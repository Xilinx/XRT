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

#include "xdp/profile/plugin/aie/aie_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/device/tracedefs.h"
#include "xdp/profile/database/database.h"

#define MAX_PATH_SZ 512

namespace xdp {

  AIEPlugin::AIEPlugin() : XDPPlugin()
  {
    db->registerPlugin(this);

    std::string version = "1.0";
    std::string creationTime = xdp::getCurrentDateTime();
    std::string xrtVersion   = xdp::getXRTVersion();
    std::string toolVersion  = xdp::getToolVersion();

    // Create writer and register it for run summary
    writers.push_back(new HALHostTraceWriter("aie_profile.csv",
                      version, creationTime, xrtVersion, toolVersion));

    (db->getStaticInfo()).addOpenedFile("aie_profile.csv", "AIE_PROFILE");
  }

  AIEPlugin::~AIEPlugin()
  {
    if (VPDatabase::alive()) {
      try {
        writeAll(false);
      }
      catch (...) {
      }
      db->unregisterPlugin(this);
    }
  }

  void AIEPlugin::writeAll(bool openNewFiles)
  {
    for (auto w : writers) {
      w->write(openNewFiles);
    }
  }
} // xdp
