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

#include "xdp/profile/plugin/hal/hal_plugin.h"
#include "xdp/profile/writer/hal/hal_host_trace_writer.h"
#include "xdp/profile/writer/hal/hal_summary_writer.h"

#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/device/tracedefs.h"

#include "xdp/profile/database/database.h"

#include "core/common/xrt_profiling.h"
#include "core/common/message.h"

#define MAX_PATH_SZ 512

namespace xdp {

  HALPlugin::HALPlugin() : XDPPlugin()
  {
    db->registerPlugin(this) ;

    std::string version = "1.1" ;

    std::string creationTime = xdp::getCurrentDateTime() ;
    std::string xrtVersion   = xdp::getXRTVersion() ;
    std::string toolVersion  = xdp::getToolVersion() ;

    // Based upon the configuration, create the appropriate writers
    writers.push_back(new HALHostTraceWriter("hal_host_trace.csv",
					     version,
					     creationTime,
					     xrtVersion,
                         toolVersion)) ;
    (db->getStaticInfo()).addOpenedFile("hal_host_trace.csv", "VP_TRACE");
#ifdef HAL_SUMMARY
    writers.push_back(new HALSummaryWriter("hal_summary.csv"));
#endif

  }

  HALPlugin::~HALPlugin()
  {
    if (VPDatabase::alive()) {
      // We were destroyed before the database, so flush our events to the 
      //  database, write the writers, and unregister ourselves from
      //  the database.
      try {
        writeAll(false);
      }
      catch (...) {
      }
      db->unregisterPlugin(this) ;
    }

    // If the database is dead, then we must have already forced a 
    //  write at the database destructor so we can just move on
  }

  void HALPlugin::writeAll(bool openNewFiles)
  {
    for (auto w : writers)
    {
      w->write(openNewFiles) ;
    }
  }
}


